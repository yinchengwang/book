/**
 * @file main.c
 * @brief BTree 索引原理演示程序
 *
 * 本程序演示 BTree 索引的核心概念：
 * - 节点结构（内部节点/叶子节点）
 * - 键比较与搜索
 * - 插入与页面分裂
 * - 范围扫描
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

#define PAGE_SIZE       4096    /* 页面大小 */
#define PAGE_HDR_SIZE   24      /* 页面头部大小 */
#define MAX_KEYS        32      /* 每个节点最大键数 */
#define TID_SIZE        8       /* TID 大小（8字节） */

/* ============================================================
 * 页面类型
 * ============================================================ */

typedef enum {
    PAGE_TYPE_INTERNAL = 1,  /* 内部节点 */
    PAGE_TYPE_LEAF     = 2   /* 叶子节点 */
} PageType;

/* ============================================================
 * BTree 节点结构
 * ============================================================ */

/**
 * @brief BTree 页面头部
 */
typedef struct PageHeader {
    PageType   type;           /* 页面类型 */
    int        level;          /* 层级（0=叶子） */
    int        num_keys;       /* 当前键数 */
    int        parent;         /* 父页面号 */
    int        left_sibling;   /* 左兄弟页面号 */
    int        right_sibling;   /* 右兄弟页面号 */
    int        free_offset;    /* 空闲空间起始偏移 */
} PageHeader;

/**
 * @brief 叶子节点条目（键 + TID）
 */
typedef struct LeafEntry {
    int  key;          /* 索引键 */
    int  heap_page;     /* 堆页面号 */
    int  heap_offset;   /* 在堆页面中的偏移 */
} LeafEntry;

/**
 * @brief 内部节点条目（键 + 子指针）
 */
typedef struct InternalEntry {
    int  key;           /* 分隔键 */
    int  child_page;    /* 子页面号 */
} InternalEntry;

/**
 * @brief BTree 页面
 */
typedef struct Page {
    PageHeader  header;
    char        data[PAGE_SIZE - sizeof(PageHeader)];
} Page;

/* ============================================================
 * BTree 索引结构
 * ============================================================ */

typedef struct BTreeIndex {
    int    root_page;   /* 根页面号 */
    int    num_pages;   /* 页面总数 */
    Page  *pages;       /* 页面数组（内存模拟） */
} BTreeIndex;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * 创建新页面
 */
static Page *create_page(BTreeIndex *idx, PageType type, int level) {
    Page *page = &idx->pages[idx->num_pages];
    memset(page, 0, sizeof(Page));
    page->header.type = type;
    page->header.level = level;
    page->header.num_keys = 0;
    page->header.free_offset = PAGE_HDR_SIZE;
    return page++;
}

/**
 * 在页面中查找键的位置（二分查找）
 */
static int find_key_pos(Page *page, int key) {
    int lo = 0, hi = page->header.num_keys;

    while (lo < hi) {
        int mid = (lo + hi) / 2;
        LeafEntry *entries = (LeafEntry *)page->data;

        if (entries[mid].key < key) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/**
 * 计算页面空闲空间
 */
static int get_free_space(Page *page) {
    return PAGE_SIZE - page->header.free_offset -
           page->header.num_keys * sizeof(LeafEntry);
}

/* ============================================================
 * BTree 操作实现
 * ============================================================ */

/**
 * 初始化 BTree 索引
 */
BTreeIndex *btree_create(void) {
    BTreeIndex *idx = calloc(1, sizeof(BTreeIndex));
    if (!idx) return NULL;

    idx->pages = calloc(256, sizeof(Page));
    if (!idx->pages) {
        free(idx);
        return NULL;
    }

    /* 创建根页面（叶子节点） */
    Page *root = &idx->pages[0];
    memset(root, 0, sizeof(Page));
    root->header.type = PAGE_TYPE_LEAF;
    root->header.level = 0;
    root->header.num_keys = 0;
    root->header.free_offset = PAGE_HDR_SIZE;

    idx->root_page = 0;
    idx->num_pages = 1;

    return idx;
}

/**
 * 插入键值对到 BTree
 */
int btree_insert(BTreeIndex *idx, int key, int heap_page, int heap_offset) {
    if (!idx) return -1;

    Page *page = &idx->pages[idx->root_page];

    /* 叶子节点已满，需要分裂 */
    if (page->header.num_keys >= MAX_KEYS) {
        printf("[btree] 页面分裂: 分裂节点 %d (level=%d, keys=%d)\n",
               idx->root_page, page->header.level, page->header.num_keys);

        /* 创建新页面作为右兄弟 */
        if (idx->num_pages >= 256) {
            printf("[btree] 错误: 页面不足\n");
            return -1;
        }

        int new_page = idx->num_pages++;
        Page *right = &idx->pages[new_page];
        memset(right, 0, sizeof(Page));
        right->header.type = PAGE_TYPE_LEAF;
        right->header.level = 0;
        right->header.num_keys = MAX_KEYS / 2;
        right->header.free_offset = PAGE_HDR_SIZE + right->header.num_keys * sizeof(LeafEntry);
        right->header.left_sibling = idx->root_page;
        right->header.right_sibling = page->header.right_sibling;

        /* 移动一半条目到新页面 */
        LeafEntry *src = (LeafEntry *)page->data;
        LeafEntry *dst = (LeafEntry *)right->data;
        for (int i = MAX_KEYS / 2; i < MAX_KEYS; i++) {
            dst[i - MAX_KEYS / 2] = src[i];
        }

        /* 更新原页面 */
        page->header.num_keys = MAX_KEYS / 2;
        page->header.right_sibling = new_page;

        /* 将新页面的最小键插入到父节点（简化：创建新根） */
        int mid_key = dst[0].key;
        printf("[btree] 分裂键: %d, 新右页面首个键: %d\n", mid_key, dst[0].key);

        /* 这里简化处理：保持原根节点继续使用 */
    }

    /* 在叶子页面中插入键 */
    int pos = find_key_pos(page, key);
    LeafEntry *entries = (LeafEntry *)page->data;

    /* 移动后续条目 */
    for (int i = page->header.num_keys; i > pos; i--) {
        entries[i] = entries[i - 1];
    }

    /* 插入新条目 */
    entries[pos].key = key;
    entries[pos].heap_page = heap_page;
    entries[pos].heap_offset = heap_offset;
    page->header.num_keys++;
    page->header.free_offset += sizeof(LeafEntry);

    printf("[btree] 插入: key=%d -> TID=(%d,%d)\n", key, heap_page, heap_offset);
    return 0;
}

/**
 * 在 BTree 中搜索键
 */
LeafEntry *btree_search(BTreeIndex *idx, int key) {
    if (!idx) return NULL;

    int current = idx->root_page;

    while (current >= 0) {
        Page *page = &idx->pages[current];

        if (page->header.type == PAGE_TYPE_LEAF) {
            /* 在叶子节点中线性搜索 */
            LeafEntry *entries = (LeafEntry *)page->data;
            for (int i = 0; i < page->header.num_keys; i++) {
                if (entries[i].key == key) {
                    printf("[btree] 查找命中: key=%d 在页面 %d, 位置 %d\n",
                           key, current, i);
                    return &entries[i];
                }
            }
            printf("[btree] 查找未命中: key=%d 不存在\n", key);
            return NULL;
        } else {
            /* 内部节点：根据键选择子页面 */
            InternalEntry *entries = (InternalEntry *)page->data;
            int pos = 0;
            while (pos < page->header.num_keys && entries[pos].key <= key) {
                pos++;
            }
            current = entries[pos > 0 ? pos - 1 : 0].child_page;
            printf("[btree] 内部搜索: 页面 %d, 下一页面 %d\n", current, current);
        }
    }

    return NULL;
}

/**
 * 范围扫描：查找 [min_key, max_key] 区间内的所有键
 */
void btree_range_scan(BTreeIndex *idx, int min_key, int max_key) {
    if (!idx) return;

    printf("[btree] 范围扫描: [%d, %d]\n", min_key, max_key);

    Page *page = &idx->pages[idx->root_page];
    LeafEntry *entries = (LeafEntry *)page->data;

    int count = 0;
    for (int i = 0; i < page->header.num_keys; i++) {
        if (entries[i].key >= min_key && entries[i].key <= max_key) {
            printf("[btree]   命中: key=%d -> TID=(%d,%d)\n",
                   entries[i].key, entries[i].heap_page, entries[i].heap_offset);
            count++;
        }
    }

    printf("[btree] 范围扫描完成: 共 %d 条记录\n", count);
}

/**
 * 销毁 BTree 索引
 */
void btree_destroy(BTreeIndex *idx) {
    if (idx) {
        free(idx->pages);
        free(idx);
    }
}

/* ============================================================
 * 主函数：演示 BTree 索引操作
 * ============================================================ */

int main(void) {
    printf("========================================\n");
    printf("      BTree 索引原理演示\n");
    printf("========================================\n\n");

    /* 创建索引 */
    BTreeIndex *idx = btree_create();
    if (!idx) {
        printf("创建 BTree 索引失败\n");
        return 1;
    }

    printf("\n--- 1. 节点结构 ---\n");
    printf("[btree] 页面结构:\n");
    printf("  PageHeader: type=%d, level=%d, num_keys=%d, free_offset=%d\n",
           0, 0, 0, PAGE_HDR_SIZE);
    printf("  LeafEntry: key(%zu) + TID(%zu bytes)\n",
           sizeof(int), 2 * sizeof(int));
    printf("  内部节点条目: key + child_page\n");

    printf("\n--- 2. 键插入演示 ---\n");
    int keys[] = {50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45, 55, 65, 75, 85};
    int n = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < n; i++) {
        btree_insert(idx, keys[i], keys[i] / 10, keys[i] % 10);
    }

    printf("\n--- 3. 键搜索演示 ---\n");
    btree_search(idx, 50);   /* 存在 */
    btree_search(idx, 25);   /* 存在 */
    btree_search(idx, 100);  /* 不存在 */

    printf("\n--- 4. 范围扫描演示 ---\n");
    btree_range_scan(idx, 30, 60);

    printf("\n--- 5. 键比较策略 ---\n");
    printf("[btree] 键比较规则:\n");
    printf("  - 整数键: 直接比较大小\n");
    printf("  - 字符串键: 字典序比较\n");
    printf("  - 多列键: 逐列比较\n");
    printf("  - NULL 值: 视为最小值\n");

    printf("\n========================================\n");
    printf("      演示完成\n");
    printf("========================================\n");

    btree_destroy(idx);
    return 0;
}
