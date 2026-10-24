/**
 * @file btree_persist.c
 * @brief B-Tree 持久化操作
 *
 * 实现 B-Tree 索引的磁盘持久化（save）和加载（load）功能。
 *
 * ## 文件格式
 *
 * ```
 * +------------------+
 * | Meta Page (0)    |  根节点位置、树参数
 * +------------------+
 * | Node Page 1      |  根节点
 * +------------------+
 * | Node Page 2      |  第二层节点...
 * +------------------+
 * | ...              |
 * +------------------+
 * | Node Page N      |  叶子节点
 * +------------------+
 * ```
 *
 * ## 持久化策略
 *
 * ### 节点到页面的映射
 * - 使用先序遍历（Pre-order Traversal）为每个节点分配 page_id
 * - page_id 从 1 开始（0 保留给 Meta Page）
 * - 保证加载时能正确恢复树结构
 *
 * ### Slotted Page 结构
 * - 每个 B-Tree 节点存储为一个 Slotted Page
 * - 内部节点存储：(right_child, key_len, value_len, key, value)
 * - 叶子节点存储：(key_len, value_len, key, value)
 * - 右孩子指针用于在内存中重建孩子指针
 *
 * ## 加载策略
 *
 * ### 递归加载
 * - 从根节点开始递归加载
 * - 每个节点加载后立即加载其孩子节点
 * - 利用先序遍历的特性：左孩子在当前节点之前加载
 *
 * ### 错误恢复
 * - 任何加载失败都会清理已分配的资源
 * - 避免悬垂指针和内存泄漏
 */

#include "btree_private.h"
#include "../shared/tree_page_private.h"

typedef struct btree_file_meta {
    uint32_t tree_type;
    uint32_t root_page_id;
    uint32_t page_count;
    uint32_t min_degree;
    uint32_t size;
    uint32_t height;
} btree_file_meta_t;

typedef struct btree_page_map_entry {
    const btree_node_t *node;
    uint32_t page_id;
} btree_page_map_entry_t;

/**
 * B-Tree 持久化辅助函数
 *
 * 这些内部函数实现节点计数、收集和查找功能，
 * 为 save/load 操作提供数据准备。
 */

/**
 * @brief 递归统计节点数量
 *
 * 遍历整棵树，统计所有节点的数量
 *
 * @param node 当前节点
 * @return 节点总数（包含当前节点）
 *
 * @note 用于 save 前预分配 page_id 数组
 * @note 空树返回 0
 *
 * ## 算法
 *
 * 后序遍历：
 * 1. 如果是叶子，返回 1
 * 2. 否则，递归统计所有子树，返回 1 + 子树总和
 */
static uint32_t _btree_count_nodes(const btree_node_t *node)
{
    uint32_t count = 1;
    uint32_t i;

    if (!node) {
        return 0;  /* 空树 */
    }

    /* 叶子节点：没有子树，直接返回 1 */
    if (node->is_leaf) {
        return 1;
    }

    /* 内部节点：递归统计所有子树 */
    for (i = 0; i <= node->nkeys; i++) {
        count += _btree_count_nodes(node->children[i]);
    }
    return count;
}

/**
 * @brief 先序遍历收集节点信息
 *
 * 按先序遍历顺序收集节点，构建 (node -> page_id) 映射表
 *
 * @param entries  节点映射表数组
 * @param cursor    当前写入位置（初始为 0）
 *
 * @note page_id 从 1 开始分配（0 保留给 meta page）
 * @note 先序遍历保证父节点在子节点之前被访问
 *
 * ## 映射表的作用
 *
 * ```
 * 遍历顺序:     A
 *             / \
 *            B   C    先序: A, B, D, E, C, F, G
 *           / \     分配:
 *          D   E     A→1, B→2, D→3, E→4, C→5, F→6, G→7
 * ```
 */
static void _btree_collect_nodes(const btree_node_t *node,
                                 btree_page_map_entry_t *entries,
                                 uint32_t *cursor)
{
    uint32_t i;

    /* 先访问当前节点 */
    entries[*cursor].node = node;
    entries[*cursor].page_id = *cursor + 1u;  /* page_id 从 1 开始 */
    (*cursor)++;

    /* 如果是叶子，遍历结束 */
    if (node->is_leaf) {
        return;
    }

    /* 递归访问子树 */
    for (i = 0; i <= node->nkeys; i++) {
        _btree_collect_nodes(node->children[i], entries, cursor);
    }
}

/**
 * @brief 在映射表中查找节点的 page_id
 *
 * @param entries  映射表
 * @param count    映射表条目数
 * @param node     待查找的节点指针
 * @return page_id，未找到返回 TREE_INDEX_INVALID_PAGE_ID
 *
 * @note 线性查找，时间复杂度 O(n)
 * @note 可以优化为 Hash 表，但节点数量通常不大
 */
static uint32_t _btree_lookup_page_id(const btree_page_map_entry_t *entries,
                                      uint32_t count,
                                      const btree_node_t *node)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        if (entries[i].node == node) {
            return entries[i].page_id;
        }
    }
    return TREE_INDEX_INVALID_PAGE_ID;
}

/**
 * @brief 将单个 B-Tree 节点写入磁盘页面
 *
 * 将内存中的 B-Tree 节点序列化为 Slotted Page 格式并写入文件
 *
 * @param file       文件指针（已打开）
 * @param page_size  页面大小
 * @param index      B-Tree 索引句柄
 * @param entries    节点映射表
 * @param entry_count 映射表条目数
 * @param map_pos    当前节点在映射表中的位置
 * @return 0 成功，-1 失败
 *
 * ## Slotted Page 格式
 *
 * ```
 * +--------------------+
 * | Page Header        |  magic, type, flags, slot_count...
 * +--------------------+
 * | Free Space         |  可用空间区域
 * +--------------------+
 * | Slot Array         |  [slot 0][slot 1]...
 * +--------------------+
 * | Data               |  [payload 0][payload 1]...
 * +--------------------+
 *         ↑
 *    slots 从后向前增长
 *    data 从前向后增长
 * ```
 *
 * ## Payload 格式
 *
 * ### 内部节点
 * ```
 * [4B right_child][4B key_len][4B value_len][key][value]
 * ```
 *
 * ### 叶子节点
 * ```
 * [4B key_len][4B value_len][key][value]
 * ```
 */
static int _btree_write_node_page(FILE *file,
                                  uint32_t page_size,
                                  const btree_index_t *index,
                                  const btree_page_map_entry_t *entries,
                                  uint32_t entry_count,
                                  uint32_t map_pos)
{
    const btree_node_t *node = entries[map_pos].node;
    tree_page_builder_t builder = {0};
    uint16_t flags = node->is_leaf ? TREE_INDEX_PAGE_FLAG_LEAF : 0u;
    uint32_t i;
    int rc = -1;

    /* 标记根页面 */
    if (node == index->root) {
        flags |= TREE_INDEX_PAGE_FLAG_ROOT;
    }

    /* 初始化 slotted page 结构
     * 分配 page_id，类型为 B-Tree 节点
     */
    if (tree_page_init(&builder,
                       page_size,
                       entries[map_pos].page_id,
                       TREE_INDEX_PAGE_BTREE_NODE,
                       flags,
                       TREE_INDEX_INVALID_PAGE_ID,      /* 前一个兄弟页面 */
                       TREE_INDEX_INVALID_PAGE_ID,      /* 后一个兄弟页面 */
                       /* 左孩子或叶子节点首页 */
                       node->is_leaf ? TREE_INDEX_INVALID_PAGE_ID :
                                       _btree_lookup_page_id(entries, entry_count, node->children[0]),
                       node->nkeys) != 0) {
        return -1;
    }

    /* 逐条写入记录 */
    for (i = 0; i < node->nkeys; i++) {
        uint32_t right_child_page = TREE_INDEX_INVALID_PAGE_ID;
        uint32_t keylen = node->records[i].keylen;
        uint32_t valuelen = node->records[i].valuelen;

        /* 计算 payload 大小 */
        uint16_t payload_len = (uint16_t)(sizeof(uint32_t) * 3u + keylen + valuelen);
        uint8_t *payload = (uint8_t *)malloc(payload_len);

        if (!payload) {
            goto cleanup;
        }

        /* 内部节点额外记录右孩子 page_id
         * 这样加载时可以重建 children[] 指针
         */
        if (!node->is_leaf) {
            right_child_page = _btree_lookup_page_id(entries, entry_count, node->children[i + 1u]);
        }

        /* 序列化 payload */
        memcpy(payload, &right_child_page, sizeof(uint32_t));
        memcpy(payload + sizeof(uint32_t), &keylen, sizeof(uint32_t));
        memcpy(payload + sizeof(uint32_t) * 2u, &valuelen, sizeof(uint32_t));
        memcpy(payload + sizeof(uint32_t) * 3u, node->records[i].key, keylen);
        if (valuelen > 0) {
            memcpy(payload + sizeof(uint32_t) * 3u + keylen, node->records[i].value, valuelen);
        }

        /* 添加到 slotted page */
        if (tree_page_add_slot(&builder, payload, payload_len) != 0) {
            free(payload);
            goto cleanup;
        }
        free(payload);
    }

    /* 完成构建并写入磁盘 */
    tree_page_finish(&builder);
    rc = tree_page_write(file, &builder);

cleanup:
    tree_page_destroy(&builder);
    return rc;
}

/**
 * @brief 递归加载 B-Tree 节点
 *
 * 从磁盘读取节点页面，反序列化为内存结构
 *
 * @param file       文件指针
 * @param page_size  页面大小
 * @param page_id    待加载的 page_id
 * @param index      B-Tree 索引句柄
 * @return 加载的节点，NULL 表示失败
 *
 * ## 加载过程
 *
 * 1. **读取页面**：从指定 page_id 读取原始数据
 * 2. **解析 Header**：提取页面类型、flags、slot_count 等
 * 3. **创建节点**：根据 is_leaf 标志创建对应类型节点
 * 4. **加载孩子**：递归加载左孩子和所有右孩子
 *
 * ## 先序遍历特性
 *
 * 利用先序遍历保存的特性：
 * - 当前节点的左孩子在当前 page_id 之前
 * - 可以直接用 page_id 加载，无需解析当前页面的孩子指针
 *
 * ```
 * 保存顺序:     A (page 1)    ← 第一个被保存
 *             / \
 *            B   C (page 2)  ← 第二个被保存
 *           / \
 *          D   E (page 3)    ← 第三个被保存
 *
 * 加载顺序:     A → B → D → E → C
 * ```
 *
 * ## 错误处理
 *
 * 任何步骤失败都会：
 * 1. 清理已分配的子节点
 * 2. 释放当前节点
 * 3. 返回 NULL
 *
 * 这避免了悬垂指针和内存泄漏。
 */
static btree_node_t *_btree_load_node(FILE *file,
                                      uint32_t page_size,
                                      uint32_t page_id,
                                      const btree_index_t *index)
{
    uint8_t *page = (uint8_t *)malloc(page_size);
    tree_index_page_header_t header;
    btree_node_t *node = NULL;
    uint32_t i;

    if (!page) {
        return NULL;
    }

    /* 第一步：读取页面并验证格式 */
    if (tree_page_read(file, page_id, page, page_size, &header) != 0 ||
        header.page_type != TREE_INDEX_PAGE_BTREE_NODE) {
        free(page);
        return NULL;
    }

    /* 第二步：创建节点结构 */
    node = _btree_node_create(index, (header.flags & TREE_INDEX_PAGE_FLAG_LEAF) != 0);
    if (!node) {
        free(page);
        return NULL;
    }
    node->nkeys = header.slot_count;

    /* 第三步：加载左孩子（如果存在） */
    if (!node->is_leaf && header.first_child_page_id != TREE_INDEX_INVALID_PAGE_ID) {
        node->children[0] = _btree_load_node(file, page_size, header.first_child_page_id, index);
        if (!node->children[0]) {
            /* 左孩子加载失败，清理当前节点 */
            _btree_node_free_shallow(node);
            free(page);
            return NULL;
        }
    }

    /* 第四步：逐条加载记录和右孩子 */
    for (i = 0; i < header.slot_count; i++) {
        const tree_index_page_slot_t *slot = tree_page_slot_at(page, &header, (uint16_t)i);
        const uint8_t *payload = tree_page_slot_payload(page, slot, page_size);
        uint32_t right_child_page;
        uint32_t keylen;
        uint32_t valuelen;

        /* 验证 slot 格式 */
        if (!slot || !payload || slot->length < sizeof(uint32_t) * 3u) {
            /* 数据损坏，清理所有已分配资源 */
            if (!node->is_leaf) {
                uint32_t j;
                for (j = 0; j <= i; j++) {
                    if (node->children[j]) {
                        _btree_node_drop(index, node->children[j]);
                        node->children[j] = NULL;
                    }
                }
            }
            _btree_node_free_shallow(node);
            free(page);
            return NULL;
        }

        /* 解析 payload */
        memcpy(&right_child_page, payload, sizeof(uint32_t));
        memcpy(&keylen, payload + sizeof(uint32_t), sizeof(uint32_t));
        memcpy(&valuelen, payload + sizeof(uint32_t) * 2u, sizeof(uint32_t));

        /* 验证 payload 完整性并设置记录 */
        if (slot->length != sizeof(uint32_t) * 3u + keylen + valuelen ||
            _btree_record_set(&node->records[i],
                              payload + sizeof(uint32_t) * 3u,
                              keylen,
                              valuelen > 0 ? payload + sizeof(uint32_t) * 3u + keylen : NULL,
                              valuelen) != 0) {
            /* 记录数据损坏，清理资源 */
            if (!node->is_leaf) {
                uint32_t j;
                for (j = 0; j <= i; j++) {
                    if (node->children[j]) {
                        _btree_node_drop(index, node->children[j]);
                        node->children[j] = NULL;
                    }
                }
            }
            _btree_node_free_shallow(node);
            free(page);
            return NULL;
        }

        /* 加载右孩子（内部节点） */
        if (!node->is_leaf && right_child_page != TREE_INDEX_INVALID_PAGE_ID) {
            node->children[i + 1u] = _btree_load_node(file, page_size, right_child_page, index);
            if (!node->children[i + 1u]) {
                /* 右孩子加载失败，清理所有孩子 */
                uint32_t j;
                for (j = 0; j <= i; j++) {
                    if (node->children[j]) {
                        _btree_node_drop(index, node->children[j]);
                        node->children[j] = NULL;
                    }
                }
                _btree_node_free_shallow(node);
                free(page);
                return NULL;
            }
        }
    }

    free(page);
    return node;
}

/**
 * @brief 保存 B-Tree 索引到磁盘
 *
 * 将内存中的 B-Tree 持久化到文件系统
 *
 * @param index    B-Tree 索引句柄
 * @param path     文件路径
 * @param options  持久化选项（可为 NULL）
 * @return 0 成功，-1 失败
 *
 * ## 持久化流程
 *
 * ```
 * +------------------+
 * | 1. 统计节点       |  _btree_count_nodes()
 * +------------------+
 * | 2. 收集节点       |  _btree_collect_nodes()
 * +------------------+
 * | 3. 写 Meta Page  |  根位置、树参数
 * +------------------+
 * | 4. 写 Node Pages |  按映射表逐个写入
 * +------------------+
 * ```
 *
 * ## 文件格式
 *
 * | Offset | Content | Size |
 * |--------|---------|------|
 * | 0      | Meta Page | page_size |
 * | page_size | Node Page 1 | page_size |
 * | 2*page_size | Node Page 2 | page_size |
 * | ... | ... | ... |
 *
 * ## Meta Page 内容
 *
 * ```c
 * typedef struct btree_file_meta {
 *     uint32_t tree_type;    // 树类型（B-Tree）
 *     uint32_t root_page_id; // 根节点 page_id（固定为 1）
 *     uint32_t page_count;   // 节点总数
 *     uint32_t min_degree;   // 最小度数
 *     uint32_t size;         // 键值对数量
 *     uint32_t height;       // 树高度
 * } btree_file_meta_t;
 * ```
 *
 * ## 原子性考虑
 *
 * 当前实现不使用 write-ahead log 或 fsync。
 * 如果写入过程中崩溃，可能导致文件损坏。
 *
 * 完整实现应使用以下策略之一：
 * 1. 先写临时文件，完成后 rename
 * 2. 使用 WAL 记录修改
 * 3. 每次写入后 fsync
 */
int btree_index_save(const btree_index_t *index,
                     const char *path,
                     const tree_index_persist_options_t *options)
{
    uint32_t page_size;
    uint32_t node_count;
    btree_page_map_entry_t *entries;
    uint32_t cursor = 0;
    FILE *file;
    tree_page_builder_t meta_builder = {0};
    btree_file_meta_t meta;
    uint32_t i;
    int rc = -1;

    if (!index || !path) {
        return -1;
    }

    /* 确定页面大小，默认使用 8KB */
    page_size = (options && options->page_size > 0) ? options->page_size : TREE_INDEX_DEFAULT_PAGE_SIZE;
    if (page_size < TREE_INDEX_MIN_PAGE_SIZE) {
        return -1;
    }

    /* ===== 步骤 1: 统计节点数 =====
     * 递归遍历整棵树，统计节点总数
     * 用于预分配 page_id 映射表
     */
    node_count = _btree_count_nodes(index->root);
    entries = (btree_page_map_entry_t *)calloc(node_count, sizeof(btree_page_map_entry_t));
    if (!entries) {
        return -1;
    }

    /* ===== 步骤 2: 收集节点 =====
     * 先序遍历，为每个节点分配 page_id
     */
    _btree_collect_nodes(index->root, entries, &cursor);

    /* 打开文件 */
    file = fopen(path, "wb+");
    if (!file) {
        free(entries);
        return -1;
    }

    /* ===== 步骤 3: 写 Meta Page =====
     * 记录树的元数据，供加载时使用
     */
    memset(&meta, 0, sizeof(meta));
    meta.tree_type = TREE_INDEX_PAGE_BTREE_NODE;
    meta.root_page_id = 1u;           /* 根节点固定在 page 1 */
    meta.page_count = node_count;     /* 总节点数 */
    meta.min_degree = index->min_degree;
    meta.size = index->size;          /* 键值对数量 */
    meta.height = btree_index_height(index);

    /* 构建并写入 meta page */
    if (tree_page_init(&meta_builder,
                       page_size,
                       0,                              /* page_id = 0 给 meta page */
                       TREE_INDEX_PAGE_META,
                       TREE_INDEX_PAGE_FLAG_ROOT,
                       TREE_INDEX_INVALID_PAGE_ID,
                       TREE_INDEX_INVALID_PAGE_ID,
                       TREE_INDEX_INVALID_PAGE_ID,
                       1) != 0 ||                     /* meta page 有 1 个 slot */
        tree_page_add_slot(&meta_builder, &meta, (uint16_t)sizeof(meta)) != 0 ||
        tree_page_write(file, &meta_builder) != 0) {
        goto cleanup;
    }

    /* ===== 步骤 4: 逐页写出所有节点 =====
     * 按映射表顺序写入，保证加载时能正确恢复
     */
    for (i = 0; i < node_count; i++) {
        if (_btree_write_node_page(file, page_size, index, entries, node_count, i) != 0) {
            goto cleanup;
        }
    }

    rc = 0;  /* 成功 */

cleanup:
    tree_page_destroy(&meta_builder);
    fclose(file);
    free(entries);
    return rc;
}

/**
 * @brief 从磁盘加载 B-Tree 索引
 *
 * 从文件系统恢复内存中的 B-Tree 结构
 *
 * @param path        文件路径
 * @param compare     键比较函数
 * @param compare_ctx 比较函数上下文
 * @return 加载的 B-Tree 索引，NULL 表示失败
 *
 * ## 加载流程
 *
 * ```
 * +------------------+
 * | 1. 读取 Meta     |  验证格式，读取树参数
 * +------------------+
 * | 2. 创建索引结构   |  按 min_degree 创建空树
 * +------------------+
 * | 3. 递归加载根节点 |  _btree_load_node()
 * +------------------+
 * | 4. 替换空根       |  用磁盘数据替换
 * +------------------+
 * ```
 *
 * ## 加载 vs 创建
 *
 * 步骤 2 创建的是"空树壳子"，只有根节点为 NULL。
 * 步骤 3 递归加载真正的根节点。
 * 步骤 4 用加载的根节点替换空壳。
 *
 * ## 为什么需要 compare 函数？
 *
 * B-Tree 的查找、插入、删除都依赖键比较。
 * 加载时需要用户提供比较函数，因为：
 * 1. 磁盘不存储比较函数指针
 * 2. 不同数据类型需要不同比较逻辑
 *
 * ## 错误处理
 *
 * - 文件不存在 → 返回 NULL
 * - 格式错误 → 返回 NULL
 * - 节点加载失败 → 清理所有资源并返回 NULL
 */
btree_index_t *btree_index_load(const char *path,
                                btree_compare_fn compare,
                                void *compare_ctx)
{
    FILE *file;
    uint8_t header_page[TREE_INDEX_MIN_PAGE_SIZE];
    tree_index_page_header_t header;
    const tree_index_page_slot_t *slot;
    const uint8_t *payload;
    btree_file_meta_t meta;
    btree_index_t *index;
    btree_node_t *root;

    if (!path) {
        return NULL;
    }

    /* 打开文件 */
    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    /* ===== 步骤 1: 读取并验证 Meta Page =====
     * 先读取固定大小的头部，验证魔数和类型
     */
    if (fread(header_page, sizeof(header_page), 1, file) != 1) {
        fclose(file);
        return NULL;
    }
    memcpy(&header, header_page, sizeof(header));

    /* 验证文件格式 */
    if (header.magic != TREE_INDEX_PAGE_MAGIC ||
        header.page_type != TREE_INDEX_PAGE_META ||
        header.page_size < TREE_INDEX_MIN_PAGE_SIZE) {
        fclose(file);
        return NULL;
    }

    /* 重新定位到文件开头 */
    if (_fseeki64(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    /* 读取完整的 meta page（可能大于最小页大小） */
    {
        uint8_t *meta_page = (uint8_t *)malloc(header.page_size);
        if (!meta_page) {
            fclose(file);
            return NULL;
        }
        if (tree_page_read(file, 0, meta_page, header.page_size, &header) != 0 ||
            header.slot_count != 1) {
            free(meta_page);
            fclose(file);
            return NULL;
        }
        /* 解析 meta slot */
        slot = tree_page_slot_at(meta_page, &header, 0);
        payload = tree_page_slot_payload(meta_page, slot, header.page_size);
        if (!slot || !payload || slot->length != sizeof(meta)) {
            free(meta_page);
            fclose(file);
            return NULL;
        }
        memcpy(&meta, payload, sizeof(meta));
        free(meta_page);
    }

    /* ===== 步骤 2: 创建空索引结构 =====
     * 按 meta 中的参数创建树框架
     */
    index = btree_index_create(meta.min_degree, compare, compare_ctx);
    if (!index) {
        fclose(file);
        return NULL;
    }

    /* ===== 步骤 3: 递归加载根节点 =====
     * 从 page 1 开始加载（根节点固定在 page 1）
     */
    root = _btree_load_node(file, header.page_size, meta.root_page_id, index);
    fclose(file);  /* 加载完成后即可关闭文件 */
    if (!root) {
        btree_index_drop(index);
        return NULL;
    }

    /* ===== 步骤 4: 替换空根 =====
     * 释放临时创建的根节点（通常是空的）
     * 用加载的根节点替换
     */
    _btree_node_drop(index, index->root);
    index->root = root;
    index->size = meta.size;

    return index;
}
