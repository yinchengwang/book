/**
 * @file yang_engine.c
 * @brief Yang 树存储引擎实现
 */
#include "yang_engine.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

#define YANG_ENGINE_NAME "yang_engine"
#define YANG_DATA_PREFIX "yang_"

/* 递归创建目录 */
static int mkpath(char *path) {
    if (path == NULL || strlen(path) == 0) return -1;

    char *p = strdup(path);
    if (p == NULL) return -1;

    char *cur = p;
    if (cur[0] == '.' && cur[1] == '/') {
        cur += 2;
    }

    for (char *sep = cur; *sep != '\0'; sep++) {
        if (*sep == '/') {
            *sep = '\0';
            if (strlen(cur) > 0 && mkdir(cur) != 0 && errno != EEXIST) {
                free(p);
                return -1;
            }
            *sep = '/';
        }
    }

    if (strlen(cur) > 0 && mkdir(cur) != 0 && errno != EEXIST) {
        free(p);
        return -1;
    }

    free(p);
    return 0;
}

static int ensure_dir(const char *path) {
    if (path == NULL) return -1;

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }

    return mkpath((char *)path);
}

typedef struct yang_engine_global_s {
    char data_dir[512];
    bool initialized;
} yang_engine_global_t;

static yang_engine_global_t g_yang_engine = {
    .data_dir = {0},
    .initialized = false
};

typedef struct yang_header_s {
    char name[256];
    uint64_t num_nodes;
} yang_header_t;

static int get_dir_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s",
             g_yang_engine.data_dir, YANG_DATA_PREFIX, name);
    return 0;
}

/* 前向声明 */
static int yang_engine_load_tree(yang_engine_db_t *db);
static int yang_engine_save_tree(yang_engine_db_t *db);

static int yang_engine_table_create(const char *name, const storage_schema_t *schema) {
    (void)schema;
    if (name == NULL) return -1;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    /* 创建目录 */
    if (ensure_dir(dir_path) != 0) {
        return -1;
    }

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    yang_header_t header = { .name = {0}, .num_nodes = 0 };
    strncpy(header.name, name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }
    return 0;
}

static void *yang_engine_table_open(const char *name, AccessMode mode) {
    if (name == NULL) return NULL;

    char meta_path[512];
    get_dir_path(name, meta_path, sizeof(meta_path));
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", meta_path);

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) return NULL;

    yang_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    yang_engine_db_t *db = (yang_engine_db_t *)calloc(1, sizeof(yang_engine_db_t));
    if (db == NULL) return NULL;

    strncpy(db->name, name, sizeof(db->name) - 1);
    get_dir_path(name, db->data_dir, sizeof(db->data_dir));
    db->mode = mode;
    db->num_nodes = header.num_nodes;
    db->root = yang_node_create("/", YANG_NODE_ROOT, NULL);

    /* 从文件加载树 */
    yang_engine_load_tree(db);

    return db;
}

static int yang_engine_table_close(void *rel) {
    if (rel == NULL) return -1;
    yang_engine_db_t *db = (yang_engine_db_t *)rel;

    /* 保存树到文件 */
    yang_engine_save_tree(db);

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", db->data_dir);

    yang_header_t header = { .num_nodes = db->num_nodes };
    strncpy(header.name, db->name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    if (db->root) yang_node_free(db->root);
    free(db);
    return 0;
}

static int yang_engine_table_drop(const char *name) {
    (void)name;
    return 0;
}

static int yang_engine_tuple_insert(void *rel, const void *data, size_t len) {
    if (rel == NULL || data == NULL) return -1;
    yang_engine_db_t *db = (yang_engine_db_t *)rel;
    if (len < sizeof(uint32_t) * 4) return -1;

    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t path_len, name_len, value_len;
    yang_node_type_t type;

    memcpy(&path_len, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    const char *path = (const char *)ptr;
    ptr += path_len;

    memcpy(&name_len, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(&type, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    memcpy(&value_len, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    const char *name = (const char *)ptr;
    ptr += name_len;

    const char *value = (const char *)ptr;

    /* 解析路径并插入节点 */
    /* 路径格式: /parent/child/grandchild */
    if (path_len == 0 || path[0] != '/') {
        return -1;
    }

    yang_node_t *current = db->root;

    /* 解析路径的每个段 */
    const char *p = path + 1;  /* 跳过开头的 '/' */
    while (*p && *p != '/') p++;  /* 跳过根路径 */

    while (*p) {
        /* 找到下一个 '/' */
        const char *seg_start = p;
        while (*p && *p != '/') p++;
        size_t seg_len = p - seg_start;

        if (seg_len == 0) {
            p++;
            continue;
        }

        /* 提取段名 */
        char seg_name[256];
        if (seg_len >= sizeof(seg_name)) seg_len = sizeof(seg_name) - 1;
        strncpy(seg_name, seg_start, seg_len);
        seg_name[seg_len] = '\0';

        /* 查找或创建子节点 */
        yang_node_t *child = yang_get_child(current, seg_name);
        if (child == NULL) {
            /* 创建新子节点 */
            child = yang_node_create(seg_name, YANG_NODE_ELEMENT, NULL);
            if (child == NULL) return -1;

            /* 构建子节点的路径 */
            char child_path[512];
            if (current->path) {
                snprintf(child_path, sizeof(child_path), "%s/%s", current->path, seg_name);
            } else {
                snprintf(child_path, sizeof(child_path), "/%s", seg_name);
            }
            child->path = strdup(child_path);

            yang_add_child(current, child);
            db->num_nodes++;
        }

        current = child;

        /* 跳过 '/' */
        while (*p == '/') p++;
    }

    /* 在目标位置创建最终节点 */
    yang_node_t *new_node = yang_node_create(name, type, value);
    if (new_node == NULL) return -1;

    /* 构建完整路径 */
    char node_path[512];
    if (current->path) {
        snprintf(node_path, sizeof(node_path), "%s/%s", current->path, name);
    } else {
        snprintf(node_path, sizeof(node_path), "/%s", name);
    }
    new_node->path = strdup(node_path);

    yang_add_child(current, new_node);
    db->num_nodes++;

    return 0;
}

static scan_desc_t *yang_engine_scan_begin(void *rel,
                                           const scan_key_t *keys, int nkeys,
                                           ScanDirection direction) {
    (void)keys;
    (void)nkeys;
    (void)direction;

    if (rel == NULL) return NULL;
    return NULL;
}

static int yang_engine_scan_next(scan_desc_t *scan, void *out_data, size_t *out_len) {
    (void)scan;
    (void)out_data;
    (void)out_len;
    return 1;
}

static int yang_engine_scan_end(scan_desc_t *scan) {
    if (scan) free(scan);
    return 0;
}

static int yang_engine_get_stats(const char *name, storage_stats_t *stats) {
    if (stats == NULL) return -1;
    memset(stats, 0, sizeof(storage_stats_t));
    if (name == NULL) return 0;

    char meta_path[512];
    get_dir_path(name, meta_path, sizeof(meta_path));
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", meta_path);

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) return -1;

    yang_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    stats->num_objects = header.num_nodes;
    return 0;
}

static const storage_ops_t g_yang_engine_ops = {
    .name = YANG_ENGINE_NAME,
    .model = MODEL_TREE,
    .init = yang_engine_init,
    .shutdown = yang_engine_shutdown,
    .table_create = yang_engine_table_create,
    .table_open = yang_engine_table_open,
    .table_close = yang_engine_table_close,
    .table_drop = yang_engine_table_drop,
    .tuple_insert = yang_engine_tuple_insert,
    .tuple_update = NULL,
    .tuple_delete = NULL,
    .scan_begin = yang_engine_scan_begin,
    .scan_next = yang_engine_scan_next,
    .scan_end = yang_engine_scan_end,
    .index_create = NULL,
    .index_drop = NULL,
    .get_stats = yang_engine_get_stats,
};

int yang_engine_init(const char *data_dir) {
    if (g_yang_engine.initialized) return 0;
    if (data_dir == NULL) return -1;
    strncpy(g_yang_engine.data_dir, data_dir, sizeof(g_yang_engine.data_dir) - 1);
    g_yang_engine.initialized = true;
    return 0;
}

int yang_engine_shutdown(void) {
    g_yang_engine.initialized = false;
    return 0;
}

const storage_ops_t *yang_engine_get_ops(void) {
    return &g_yang_engine_ops;
}

int yang_engine_create(const char *name, const storage_schema_t *schema) {
    return yang_engine_table_create(name, schema);
}

void *yang_engine_open(const char *name, AccessMode mode) {
    return yang_engine_table_open(name, mode);
}

int yang_engine_close(void *rel) {
    return yang_engine_table_close(rel);
}

int yang_engine_drop(const char *name) {
    return yang_engine_table_drop(name);
}

int yang_engine_insert(void *rel, const void *data, size_t len) {
    return yang_engine_tuple_insert(rel, data, len);
}

int yang_engine_stats(const char *name, storage_stats_t *stats) {
    return yang_engine_get_stats(name, stats);
}

yang_node_t *yang_node_create(const char *name, yang_node_type_t type,
                               const char *value) {
    yang_node_t *node = (yang_node_t *)calloc(1, sizeof(yang_node_t));
    if (node == NULL) return NULL;

    if (name) {
        node->name = strdup(name);
        if (node->name == NULL) {
            free(node);
            return NULL;
        }
    }

    node->type = type;

    if (value) {
        node->value = strdup(value);
        if (node->value == NULL) {
            free(node->name);
            free(node);
            return NULL;
        }
    }

    return node;
}

void yang_node_free(yang_node_t *node) {
    if (node == NULL) return;

    yang_node_t *child = node->first_child;
    while (child) {
        yang_node_t *next = child->next_sibling;
        yang_node_free(child);
        child = next;
    }

    if (node->path) free(node->path);
    if (node->name) free(node->name);
    if (node->value) free(node->value);
    free(node);
}

int yang_add_child(yang_node_t *parent, yang_node_t *child) {
    if (parent == NULL || child == NULL) return -1;

    child->parent = parent;

    if (parent->first_child == NULL) {
        parent->first_child = child;
    } else {
        yang_node_t *last = parent->first_child;
        while (last->next_sibling) {
            last = last->next_sibling;
        }
        last->next_sibling = child;
    }

    return 0;
}

yang_node_t *yang_get_child(const yang_node_t *node, const char *name) {
    if (node == NULL || name == NULL) return NULL;

    yang_node_t *child = node->first_child;
    while (child) {
        if (child->name && strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

/* ========================================================================
 * 树持久化实现
 * ======================================================================== */

/**
 * @brief 序列化节点到文件
 */
static int serialize_node(FILE *fp, yang_node_t *node, int depth) {
    if (!fp || !node) return -1;

    /* 写入缩进 */
    for (int i = 0; i < depth; i++) {
        fprintf(fp, "  ");
    }

    /* 写入节点信息 */
    fprintf(fp, "%s", node->name ? node->name : "");

    if (node->type == YANG_NODE_ATTRIBUTE) {
        fprintf(fp, "=\"%s\"", node->value ? node->value : "");
    } else if (node->value) {
        fprintf(fp, " \"%s\"", node->value);
    }

    /* 写入类型标记 */
    fprintf(fp, " [%d]", node->type);

    /* 写入路径 */
    fprintf(fp, " @%s", node->path ? node->path : "/");

    /* 检查是否有子节点 */
    if (node->first_child) {
        fprintf(fp, " {\n");
        yang_node_t *child = node->first_child;
        while (child) {
            serialize_node(fp, child, depth + 1);
            child = child->next_sibling;
        }
        for (int i = 0; i < depth; i++) {
            fprintf(fp, "  ");
        }
        fprintf(fp, "}\n");
    } else {
        fprintf(fp, ";\n");
    }

    return 0;
}

/**
 * @brief 保存树到文件
 */
static int yang_engine_save_tree(yang_engine_db_t *db) {
    if (!db || !db->root) return -1;

    char tree_path[512];
    snprintf(tree_path, sizeof(tree_path), "%s/tree.yang", db->data_dir);

    FILE *fp = fopen(tree_path, "w");
    if (!fp) return -1;

    /* 写入头部注释 */
    fprintf(fp, "# Yang Tree Data File\n");
    fprintf(fp, "# Version: 1.0\n");
    fprintf(fp, "# Node Count: %lu\n\n", (unsigned long)db->num_nodes);

    /* 序列化根节点的子节点 */
    yang_node_t *child = db->root->first_child;
    while (child) {
        serialize_node(fp, child, 0);
        child = child->next_sibling;
    }

    fclose(fp);
    return 0;
}

/**
 * @brief 解析一行节点数据
 */
static yang_node_t *parse_node_line(const char *line, int *out_depth) {
    if (!line || !out_depth) return NULL;

    /* 计算缩进深度 */
    *out_depth = 0;
    const char *p = line;
    while (*p == ' ') {
        (*out_depth)++;
        p++;
    }

    if (*p == '#' || *p == '\0') {
        return NULL;  /* 注释或空行 */
    }

    /* 解析节点名 */
    const char *name_start = p;
    while (*p && !isspace(*p) && *p != '[' && *p != ';') p++;
    size_t name_len = p - name_start;

    if (name_len == 0) return NULL;

    char name[256];
    strncpy(name, name_start, name_len);
    name[name_len] = '\0';

    /* 解析属性值 */
    char value[512] = {0};
    if (*p == ' ') {
        p++;
        if (*p == '"') {
            p++;
            const char *val_start = p;
            while (*p && *p != '"') p++;
            size_t val_len = p - val_start;
            if (val_len < sizeof(value)) {
                strncpy(value, val_start, val_len);
                value[val_len] = '\0';
            }
            if (*p == '"') p++;
        }
    }

    /* 解析类型和路径 */
    yang_node_type_t type = YANG_NODE_ELEMENT;
    char path[512] = {0};

    while (*p) {
        while (*p == ' ') p++;

        if (*p == '[') {
            /* 类型标记 */
            p++;
            type = (yang_node_type_t)atoi(p);
            while (*p && *p != ']') p++;
            if (*p == ']') p++;
        } else if (*p == '@') {
            /* 路径标记 */
            p++;
            const char *path_start = p;
            while (*p && *p != ';' && *p != ' ' && *p != '{') p++;
            size_t path_len = p - path_start;
            if (path_len < sizeof(path)) {
                strncpy(path, path_start, path_len);
                path[path_len] = '\0';
            }
        } else {
            p++;
        }
    }

    /* 创建节点 */
    yang_node_t *node = yang_node_create(name, type, value[0] ? value : NULL);
    if (node) {
        node->path = strdup(path[0] ? path : "/");
    }

    return node;
}

/**
 * @brief 从文件加载树
 */
static int yang_engine_load_tree(yang_engine_db_t *db) {
    if (!db) return -1;

    char tree_path[512];
    snprintf(tree_path, sizeof(tree_path), "%s/tree.yang", db->data_dir);

    FILE *fp = fopen(tree_path, "r");
    if (!fp) {
        /* 文件不存在是正常的，新树没有数据 */
        return 0;
    }

    /* 释放现有根节点（但保留空根） */
    if (db->root) {
        yang_node_t *child = db->root->first_child;
        while (child) {
            yang_node_t *next = child->next_sibling;
            yang_node_free(child);
            child = next;
        }
        db->root->first_child = NULL;
    }

    /* 解析文件 */
    char line[1024];
    yang_node_t *stack[64] = {0};  /* 深度栈 */
    int stack_depth = 0;

    stack[0] = db->root;

    while (fgets(line, sizeof(line), fp)) {
        /* 去除换行符 */
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[len-1] = '\0';
            if (len > 1 && line[len-2] == '\r') line[len-2] = '\0';
        }

        int depth;
        yang_node_t *node = parse_node_line(line, &depth);
        if (!node) continue;

        /* 更新栈深度 */
        while (stack_depth > depth) {
            stack_depth--;
        }

        /* 添加到父节点 */
        yang_add_child(stack[stack_depth], node);

        /* 检查是否有子节点（行的下一部分） */
        /* 如果是复合节点，压入栈 */
        char *brace = strstr(line, " {");
        if (brace) {
            stack[++stack_depth] = node;
        }
    }

    fclose(fp);

    /* 重新计算节点数 */
    /* 简化处理：依赖 header 中的 num_nodes */

    return 0;
}

int yang_engine_get(void *rel, const void *key, size_t key_len,
                    void **out_data, size_t *out_len) {
    if (!rel || !key || key_len == 0) return -1;

    yang_engine_db_t *db = (yang_engine_db_t *)rel;
    const char *path = (const char *)key;

    /* 解析路径查找节点 */
    yang_node_t *current = db->root;

    /* 跳过开头的 '/' */
    const char *p = path;
    while (*p == '/') p++;

    while (*p && current) {
        /* 找到下一个 '/' */
        const char *seg_start = p;
        while (*p && *p != '/') p++;
        size_t seg_len = p - seg_start;

        if (seg_len == 0) break;

        char seg_name[256];
        if (seg_len >= sizeof(seg_name)) seg_len = sizeof(seg_name) - 1;
        strncpy(seg_name, seg_start, seg_len);
        seg_name[seg_len] = '\0';

        current = yang_get_child(current, seg_name);

        while (*p == '/') p++;
    }

    if (!current || current == db->root) {
        return 1;  /* 未找到 */
    }

    /* 序列化节点数据 */
    size_t buf_size = 1024;
    uint8_t *buf = (uint8_t *)malloc(buf_size);
    if (!buf) return -1;

    size_t offset = 0;

    /* 写入类型 */
    uint32_t type = current->type;
    memcpy(buf + offset, &type, sizeof(type));
    offset += sizeof(type);

    /* 写入值 */
    const char *val = current->value ? current->value : "";
    uint32_t val_len = (uint32_t)strlen(val) + 1;
    if (offset + sizeof(uint32_t) + val_len > buf_size) {
        buf_size = offset + sizeof(uint32_t) + val_len + 256;
        uint8_t *new_buf = (uint8_t *)realloc(buf, buf_size);
        if (!new_buf) {
            free(buf);
            return -1;
        }
        buf = new_buf;
    }
    memcpy(buf + offset, &val_len, sizeof(val_len));
    offset += sizeof(val_len);
    memcpy(buf + offset, val, val_len);
    offset += val_len;

    *out_data = buf;
    *out_len = offset;
    return 0;
}

yang_node_t *yang_engine_find(void *rel, const char *path) {
    if (!rel || !path) return NULL;

    yang_engine_db_t *db = (yang_engine_db_t *)rel;
    yang_node_t *current = db->root;

    /* 跳过开头的 '/' */
    const char *p = path;
    while (*p == '/') p++;

    while (*p && current) {
        /* 找到下一个 '/' */
        const char *seg_start = p;
        while (*p && *p != '/') p++;
        size_t seg_len = p - seg_start;

        if (seg_len == 0) break;

        char seg_name[256];
        if (seg_len >= sizeof(seg_name)) seg_len = sizeof(seg_name) - 1;
        strncpy(seg_name, seg_start, seg_len);
        seg_name[seg_len] = '\0';

        current = yang_get_child(current, seg_name);

        while (*p == '/') p++;
    }

    return (current == db->root) ? NULL : current;
}

int yang_engine_traverse(void *rel,
                         bool (*callback)(const char *path, yang_node_t *node, void *ctx),
                         void *ctx) {
    if (!rel || !callback) return -1;

    yang_engine_db_t *db = (yang_engine_db_t *)rel;

    /* 递归遍历树 */
    int count = 0;

    typedef struct {
        yang_node_t *node;
        bool entered;
    } traverse_stack_t;

    traverse_stack_t stack[64];
    int stack_top = 0;

    stack[0].node = db->root;
    stack[0].entered = false;

    while (stack_top >= 0) {
        traverse_stack_t *top = &stack[stack_top];

        if (!top->entered) {
            top->entered = true;
            /* 调用回调（进入节点） */
            if (top->node != db->root) {
                if (!callback(top->node->path, top->node, ctx)) {
                    return count;
                }
                count++;
            }

            /* 将子节点压栈（反向，压第一个） */
            if (top->node->first_child) {
                stack_top++;
                stack[stack_top].node = top->node->first_child;
                stack[stack_top].entered = false;
            }
        } else {
            /* 进入兄弟节点 */
            if (top->node->next_sibling) {
                stack[stack_top].node = top->node->next_sibling;
                stack[stack_top].entered = false;
            } else {
                /* 弹出栈 */
                stack_top--;
            }
        }
    }

    return count;
}
