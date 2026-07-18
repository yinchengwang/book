/**
 * @file yang_tree.c
 * @brief Yang 树模型查询实现
 */

#include "db/storage/yang/yang_tree.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static char **split_path(const char *path, const char *sep, size_t *out_count) {
    if (!path || !out_count) return NULL;

    size_t capacity = 16;
    char **segments = (char **)calloc(capacity, sizeof(char *));
    size_t count = 0;

    const char *p = path;
    const char *start = p;

    while (*p) {
        if (strncmp(p, sep, strlen(sep)) == 0) {
            if (start < p) {
                if (count >= capacity) {
                    capacity *= 2;
                    segments = (char **)realloc(segments, capacity * sizeof(char *));
                }
                size_t len = p - start;
                segments[count] = (char *)malloc(len + 1);
                memcpy(segments[count], start, len);
                segments[count][len] = '\0';
                count++;
            }
            p += strlen(sep);
            start = p;
        } else {
            p++;
        }
    }

    /* 最后一个段 */
    if (start < p) {
        if (count >= capacity) {
            capacity *= 2;
            segments = (char **)realloc(segments, capacity * sizeof(char *));
        }
        size_t len = p - start;
        segments[count] = (char *)malloc(len + 1);
        memcpy(segments[count], start, len);
        segments[count][len] = '\0';
        count++;
    }

    *out_count = count;
    return segments;
}

static void free_segments(char **segments, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(segments[i]);
    }
    free(segments);
}

static char *join_path(char **segments, size_t count, const char *sep) {
    if (!segments || count == 0) return strdup("");

    size_t total_len = 0;
    for (size_t i = 0; i < count; i++) {
        total_len += strlen(segments[i]);
    }
    total_len += strlen(sep) * (count > 0 ? count - 1 : 0) + 1;

    char *result = (char *)malloc(total_len);
    char *p = result;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) p += sprintf(p, "%s", sep);
        p += sprintf(p, "%s", segments[i]);
    }
    return result;
}

static yang_node_t *find_node_by_path(yang_node_t *root, const char *path) {
    if (!root || !path) return NULL;

    size_t num_segs;
    char **segs = split_path(path, "/", &num_segs);
    if (num_segs == 0) {
        free_segments(segs, 0);
        return NULL;
    }

    yang_node_t *current = root;
    for (size_t i = 0; i < num_segs && current; i++) {
        if (strcmp(segs[i], "") == 0) continue;
        yang_node_t *child = yang_get_child(current, segs[i]);
        current = child;
    }

    free_segments(segs, num_segs);
    return current;
}

/* ========================================================================
 * 祖先查询
 * ======================================================================== */

YangAncestor *yang_get_ancestors(void *tree,
                                const char *path,
                                bool include_self,
                                size_t *out_count) {
    if (!tree || !path || !out_count) return NULL;

    size_t num_segs;
    char **segs = split_path(path, "/", &num_segs);
    if (num_segs == 0) {
        free_segments(segs, 0);
        *out_count = 0;
        return NULL;
    }

    /* 计算祖先数量 */
    size_t start_idx = include_self ? 0 : num_segs - 1;
    size_t anc_count = include_self ? num_segs : num_segs - 1;

    YangAncestor *ancestors = (YangAncestor *)calloc(anc_count, sizeof(YangAncestor));
    yang_node_t *root = ((yang_engine_db_t *)tree)->root;

    for (size_t i = start_idx; i < num_segs; i++) {
        YangAncestor *anc = &ancestors[i - start_idx];

        /* 构建路径 */
        char **path_segs = (char **)malloc((i + 1) * sizeof(char *));
        for (size_t j = 0; j <= i; j++) {
            path_segs[j] = segs[j];
        }
        anc->path = join_path(path_segs, i + 1, "/");
        free(path_segs);

        anc->name = strdup(segs[i]);
        anc->depth = (int)i;

        /* 查找节点获取类型 */
        yang_node_t *node = find_node_by_path(root, anc->path);
        anc->type = node ? node->type : YANG_NODE_ELEMENT;
    }

    free_segments(segs, num_segs);
    *out_count = anc_count;
    return ancestors;
}

yang_node_t *yang_get_parent(void *tree, const char *path) {
    if (!tree || !path) return NULL;

    char *parent_path = yang_parent_path(path, "/");
    if (!parent_path) return NULL;

    yang_engine_db_t *db = (yang_engine_db_t *)tree;
    yang_node_t *parent = find_node_by_path(db->root, parent_path);

    free(parent_path);
    return parent;
}

char **yang_get_ancestor_paths(void *tree,
                               const char *path,
                               const char *separator,
                               size_t *out_count) {
    if (!path || !out_count) return NULL;

    size_t num_segs;
    char **segs = split_path(path, "/", &num_segs);
    if (num_segs == 0) {
        free_segments(segs, 0);
        *out_count = 0;
        return NULL;
    }

    char **paths = (char **)malloc(num_segs * sizeof(char *));
    size_t count = 0;

    for (size_t i = 0; i < num_segs; i++) {
        char **path_segs = (char **)malloc((i + 1) * sizeof(char *));
        for (size_t j = 0; j <= i; j++) {
            path_segs[j] = segs[j];
        }
        paths[count++] = join_path(path_segs, i + 1, separator ? separator : "/");
        free(path_segs);
    }

    free_segments(segs, num_segs);
    *out_count = count;
    return paths;
}

void yang_free_ancestors(YangAncestor *ancestors, size_t count) {
    if (!ancestors) return;
    for (size_t i = 0; i < count; i++) {
        free(ancestors[i].path);
        free(ancestors[i].name);
    }
    free(ancestors);
}

/* ========================================================================
 * 后代查询
 * ======================================================================== */

static void collect_descendants(yang_node_t *node, int current_depth,
                               int max_depth,
                               YangDescendant **results,
                               size_t *count, size_t *capacity) {
    if (!node) return;

    yang_node_t *child = node->first_child;
    while (child) {
        if (*count >= *capacity) {
            *capacity *= 2;
            *results = (YangDescendant *)realloc(*results,
                *capacity * sizeof(YangDescendant));
        }

        YangDescendant *desc = &((*results)[*count]);
        desc->path = strdup(child->path);
        desc->name = strdup(child->name);
        desc->type = child->type;
        desc->depth = current_depth + 1;
        (*count)++;

        if (max_depth < 0 || current_depth + 1 < max_depth) {
            collect_descendants(child, current_depth + 1, max_depth,
                               results, count, capacity);
        }

        child = child->next_sibling;
    }
}

YangDescendant *yang_get_descendants(void *tree,
                                     const char *path,
                                     int max_depth,
                                     size_t *out_count) {
    if (!tree || !path || !out_count) return NULL;

    yang_engine_db_t *db = (yang_engine_db_t *)tree;
    yang_node_t *start_node = find_node_by_path(db->root, path);
    if (!start_node) {
        *out_count = 0;
        return NULL;
    }

    size_t capacity = 64;
    YangDescendant *results = (YangDescendant *)calloc(capacity, sizeof(YangDescendant));
    size_t count = 0;

    collect_descendants(start_node, 0, max_depth, &results, &count, &capacity);

    *out_count = count;
    return results;
}

yang_node_t **yang_get_children(void *tree,
                                const char *path,
                                size_t *out_count) {
    if (!tree || !path || !out_count) return NULL;

    yang_engine_db_t *db = (yang_engine_db_t *)tree;
    yang_node_t *node = find_node_by_path(db->root, path);
    if (!node || !node->first_child) {
        *out_count = 0;
        return NULL;
    }

    /* 计算子节点数量 */
    size_t count = 0;
    yang_node_t *child = node->first_child;
    while (child) {
        count++;
        child = child->next_sibling;
    }

    yang_node_t **children = (yang_node_t **)malloc(count * sizeof(yang_node_t *));
    child = node->first_child;
    for (size_t i = 0; i < count; i++) {
        children[i] = child;
        child = child->next_sibling;
    }

    *out_count = count;
    return children;
}

size_t yang_delete_subtree(void *tree, const char *path) {
    if (!tree || !path) return 0;

    yang_engine_db_t *db = (yang_engine_db_t *)tree;
    yang_node_t *parent = yang_get_parent(tree, path);
    if (!parent) return 0;

    /* 找到要删除的节点 */
    yang_node_t *child = parent->first_child;
    yang_node_t *prev = NULL;

    char *leaf = yang_path_leaf(path, "/");
    while (child) {
        if (strcmp(child->name, leaf) == 0) {
            /* 从父节点移除 */
            if (prev) {
                prev->next_sibling = child->next_sibling;
            } else {
                parent->first_child = child->next_sibling;
            }

            /* 递归释放子节点 */
            size_t deleted = 1;
            yang_node_t *sub = child->first_child;
            while (sub) {
                char sub_path[512];
                snprintf(sub_path, sizeof(sub_path), "%s/%s", child->path, sub->name);
                deleted += yang_delete_subtree(tree, sub_path);
                sub = sub->next_sibling;
            }

            yang_node_free(child);
            free(leaf);
            db->num_nodes--;
            return deleted;
        }
        prev = child;
        child = child->next_sibling;
    }

    free(leaf);
    return 0;
}

void yang_free_descendants(YangDescendant *descendants, size_t count) {
    if (!descendants) return;
    for (size_t i = 0; i < count; i++) {
        free(descendants[i].path);
        free(descendants[i].name);
    }
    free(descendants);
}

/* ========================================================================
 * 深度和路径查询
 * ======================================================================== */

int yang_get_depth(void *tree, const char *path) {
    return yang_path_depth(path, "/");
}

int yang_path_depth(const char *path, const char *separator) {
    if (!path) return -1;

    size_t count;
    char **segs = split_path(path, separator ? separator : "/", &count);

    /* 过滤空段 */
    int depth = 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(segs[i], "") != 0) {
            depth++;
        }
    }

    free_segments(segs, count);
    return depth - 1;  /* 根深度为 0 */
}

char *yang_parent_path(const char *path, const char *separator) {
    if (!path) return NULL;

    const char *sep = separator ? separator : "/";
    const char *last_sep = NULL;
    const char *p = path + strlen(path) - 1;

    /* 跳过尾部分隔符 */
    while (p > path && strncmp(p, sep, strlen(sep)) == 0) p--;

    while (p >= path) {
        if (strncmp(p, sep, strlen(sep)) == 0) {
            last_sep = p;
            break;
        }
        p--;
    }

    if (!last_sep) return strdup("");

    size_t len = last_sep - path;
    char *result = (char *)malloc(len + 1);
    memcpy(result, path, len);
    result[len] = '\0';
    return result;
}

char *yang_path_leaf(const char *path, const char *separator) {
    if (!path) return NULL;

    const char *sep = separator ? separator : "/";
    size_t sep_len = strlen(sep);

    const char *last_sep = NULL;
    const char *p = path + strlen(path) - 1;

    /* 跳过尾部分隔符 */
    while (p > path && strncmp(p, sep, sep_len) == 0) p--;

    while (p >= path) {
        if (strncmp(p, sep, sep_len) == 0) {
            last_sep = p + sep_len;
            break;
        }
        p--;
    }

    if (!last_sep) return strdup(path);

    return strdup(last_sep);
}

YangPathInfo *yang_get_path_info(void *tree, const char *path) {
    if (!path) return NULL;

    YangPathInfo *info = (YangPathInfo *)calloc(1, sizeof(YangPathInfo));
    if (!info) return NULL;

    info->segments = split_path(path, "/", &info->num_segments);
    info->full_path = join_path(info->segments, info->num_segments, "/");
    info->depth = yang_path_depth(path, "/");

    (void)tree;
    return info;
}

void yang_free_path_info(YangPathInfo *info) {
    if (!info) return;
    free(info->full_path);
    free_segments(info->segments, info->num_segments);
    free(info);
}

/* ========================================================================
 * 路径匹配
 * ======================================================================== */

/* 前缀匹配回调上下文 */
typedef struct {
    const char *prefix;
    size_t prefix_len;
    YangPatternMatch *matches;
    size_t count;
    size_t capacity;
} prefix_match_ctx_t;

static bool prefix_match_callback(const char *path, yang_node_t *node, void *ctx)
{
    prefix_match_ctx_t *pc = (prefix_match_ctx_t *)ctx;
    if (strncmp(path, pc->prefix, pc->prefix_len) == 0) {
        size_t idx = pc->count;
        if (idx >= pc->capacity) {
            pc->capacity *= 2;
            YangPatternMatch *new_matches = (YangPatternMatch *)realloc(pc->matches,
                pc->capacity * sizeof(YangPatternMatch));
            if (!new_matches) return false;
            pc->matches = new_matches;
        }
        pc->matches[idx].path = strdup(path);
        pc->matches[idx].node = node;
        pc->matches[idx].depth = yang_path_depth(path, "/");
        pc->count++;
    }
    (void)node;
    return true;
}

YangPatternMatch *yang_match_prefix(void *tree,
                                   const char *prefix,
                                   size_t *out_count) {
    if (!tree || !prefix || !out_count) return NULL;

    YangPatternMatch *matches = (YangPatternMatch *)calloc(64, sizeof(YangPatternMatch));

    prefix_match_ctx_t ctx;
    ctx.prefix = prefix;
    ctx.prefix_len = strlen(prefix);
    ctx.matches = matches;
    ctx.count = 0;
    ctx.capacity = 64;

    yang_engine_traverse(tree, prefix_match_callback, &ctx);

    *out_count = ctx.count;
    return ctx.matches;
}

YangPatternMatch *yang_match_wildcard(void *tree,
                                     const char *pattern,
                                     size_t *out_count) {
    if (!tree || !pattern || !out_count) return NULL;

    /* 简化实现：使用前缀匹配 */
    if (pattern[0] == '*' && pattern[1] == '*') {
        return yang_match_prefix(tree, "", out_count);
    }

    /* 提取前缀部分 */
    char prefix[256];
    const char *p = pattern;
    size_t i = 0;

    while (*p && *p != '*' && i < sizeof(prefix) - 1) {
        prefix[i++] = *p++;
    }
    prefix[i] = '\0';

    return yang_match_prefix(tree, prefix, out_count);
}

void yang_free_matches(YangPatternMatch *matches, size_t count) {
    if (!matches) return;
    for (size_t i = 0; i < count; i++) {
        free(matches[i].path);
    }
    free(matches);
}

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

char *yang_sql_ancestors(const char *path) {
    size_t count;
    YangAncestor *ancestors = yang_get_ancestors(NULL, path, true, &count);

    char *result = (char *)malloc(4096);
    char *p = result;
    p += sprintf(p, "[");

    for (size_t i = 0; i < count; i++) {
        if (i > 0) p += sprintf(p, ",");
        p += sprintf(p, "{\"path\":\"%s\",\"name\":\"%s\",\"depth\":%d}",
                    ancestors[i].path, ancestors[i].name, ancestors[i].depth);
    }

    p += sprintf(p, "]");
    yang_free_ancestors(ancestors, count);
    return result;
}

char *yang_sql_descendants(const char *path, int max_depth) {
    size_t count;
    YangDescendant *descendants = yang_get_descendants(NULL, path, max_depth, &count);

    char *result = (char *)malloc(4096);
    char *p = result;
    p += sprintf(p, "[");

    for (size_t i = 0; i < count; i++) {
        if (i > 0) p += sprintf(p, ",");
        p += sprintf(p, "{\"path\":\"%s\",\"name\":\"%s\",\"depth\":%d}",
                    descendants[i].path, descendants[i].name, descendants[i].depth);
    }

    p += sprintf(p, "]");
    yang_free_descendants(descendants, count);
    return result;
}

int yang_sql_depth(const char *path) {
    return yang_path_depth(path, "/");
}

char *yang_sql_path(const char *path, const char *separator) {
    return yang_parent_path(path, separator);
}

char **yang_sql_level_path(const char *root, int level, size_t *out_count) {
    if (!root || level < 0 || !out_count) return NULL;

    size_t root_depth = yang_path_depth(root, "/");
    size_t count;
    YangDescendant *descendants = yang_get_descendants(NULL, root, -1, &count);

    char **results = (char **)malloc(count * sizeof(char *));
    size_t num_results = 0;

    for (size_t i = 0; i < count; i++) {
        if ((int)descendants[i].depth == root_depth + level) {
            results[num_results++] = strdup(descendants[i].path);
        }
    }

    yang_free_descendants(descendants, count);
    *out_count = num_results;
    return results;
}

char *yang_sql_xpath(const char *path, const char *pattern) {
    size_t count;
    YangPatternMatch *matches = yang_match_wildcard(NULL, pattern, &count);

    char *result = (char *)malloc(4096);
    char *p = result;
    p += sprintf(p, "[");

    for (size_t i = 0; i < count; i++) {
        if (i > 0) p += sprintf(p, ",");
        p += sprintf(p, "\"%s\"", matches[i].path);
    }

    p += sprintf(p, "]");
    yang_free_matches(matches, count);
    return result;
}

/* ========================================================================
 * 树操作
 * ======================================================================== */

size_t yang_copy_subtree(void *tree, const char *src_path, const char *dest_path) {
    (void)tree; (void)src_path; (void)dest_path;
    return 0;
}

int yang_move_subtree(void *tree, const char *src_path, const char *dest_path) {
    (void)tree; (void)src_path; (void)dest_path;
    return -1;
}

void yang_get_tree_stats(void *tree, int *out_depth, size_t *out_nodes) {
    if (!tree) return;

    yang_engine_db_t *db = (yang_engine_db_t *)tree;
    if (out_nodes) *out_nodes = db->num_nodes;
    if (out_depth) *out_depth = 0;
}
