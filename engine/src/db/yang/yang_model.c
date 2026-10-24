/**
 * @file yang_model.c
 * @brief YANG 模型解析器实现
 *
 * 解析 YANG 简化子集，构建 schema 树。
 * 词法/语法均为手写递归下降，复杂度可控。
 *
 * 支持语句：
 *   module <name> { ... }
 *   container <name> { ... }
 *   leaf <name> { type <type>; ... }
 *   leaf-list <name> { type <type>; ... }
 *   list <name> { key "k1 k2"; ... }
 *   typedef <name> { type <type>; }
 *   grouping <name> { ... }
 *   uses <name>;
 *   type <type>;
 *   must "expr";
 *   when "expr";
 *   default "<value>";
 *   mandatory true;
 *   config false;
 *   description "text";
 *   reference "text";
 */
#include "db/yang/yang_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================
 * 辅助宏
 * ============================================================ */

#define SET_STR(dst, src)                                   \
    do {                                                    \
        if ((src) != NULL) {                                \
            strncpy((dst), (src), sizeof(dst) - 1);         \
            (dst)[sizeof(dst) - 1] = '\0';                 \
        } else {                                            \
            (dst)[0] = '\0';                                \
        }                                                   \
    } while (0)

/* ============================================================
 * 词法：游标
 * ============================================================ */

typedef struct yang_lexer_s {
    const char *src;        /**< 源文本 */
    size_t pos;             /**< 当前偏移 */
    size_t len;             /**< 文本长度 */
} yang_lexer_t;

static void lex_init(yang_lexer_t *l, const char *src, size_t len) {
    if (len == 0) len = strlen(src);
    l->src = src;
    l->pos = 0;
    l->len = len;
}

static void skip_ws_and_comments(yang_lexer_t *l) {
    while (l->pos < l->len) {
        char c = l->src[l->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            l->pos++;
        } else if (c == '/' && l->pos + 1 < l->len && l->src[l->pos + 1] == '/') {
            while (l->pos < l->len && l->src[l->pos] != '\n') l->pos++;
        } else if (c == '/' && l->pos + 1 < l->len && l->src[l->pos + 1] == '*') {
            l->pos += 2;
            while (l->pos + 1 < l->len &&
                   !(l->src[l->pos] == '*' && l->src[l->pos + 1] == '/')) {
                l->pos++;
            }
            if (l->pos + 1 < l->len) l->pos += 2;
        } else {
            break;
        }
    }
}

/** 读取一个标识符；返回是否成功 */
static int lex_ident(yang_lexer_t *l, char *buf, size_t buf_len) {
    skip_ws_and_comments(l);
    size_t start = l->pos;
    if (start >= l->len) return 0;
    char c = l->src[start];
    if (!(isalpha((unsigned char)c) || c == '_' || c == '-')) return 0;
    while (l->pos < l->len) {
        char cc = l->src[l->pos];
        if (isalnum((unsigned char)cc) || cc == '_' || cc == '-' || cc == '.') {
            l->pos++;
        } else {
            break;
        }
    }
    size_t n = l->pos - start;
    if (n + 1 > buf_len) n = buf_len - 1;
    memcpy(buf, l->src + start, n);
    buf[n] = '\0';
    return 1;
}

/** 期望下一个标识符匹配 kw；不匹配返回 -1 */
static int lex_expect(yang_lexer_t *l, const char *kw) {
    char buf[YANG_MAX_NAME_LEN];
    if (!lex_ident(l, buf, sizeof(buf))) return -1;
    if (strcmp(buf, kw) == 0) return 0;
    return -1;
}

/** 期望 ';' 或 '{' 或 '}' */
static int lex_punct(yang_lexer_t *l, char p) {
    skip_ws_and_comments(l);
    if (l->pos < l->len && l->src[l->pos] == p) {
        l->pos++;
        return 0;
    }
    return -1;
}

/** 读取带引号字符串（双引号）；返回写入长度 */
static int lex_quoted(yang_lexer_t *l, char *buf, size_t buf_len) {
    skip_ws_and_comments(l);
    if (l->pos >= l->len || l->src[l->pos] != '"') return -1;
    l->pos++;
    size_t i = 0;
    while (l->pos < l->len && l->src[l->pos] != '"') {
        if (l->src[l->pos] == '\\' && l->pos + 1 < l->len) {
            char esc = l->src[l->pos + 1];
            const char *repl = NULL;
            switch (esc) {
                case 'n': repl = "\n"; break;
                case 't': repl = "\t"; break;
                case '"': repl = "\""; break;
                case '\\': repl = "\\"; break;
                default: repl = NULL;
            }
            if (repl) {
                if (i + 1 < buf_len) buf[i++] = *repl;
                l->pos += 2;
                continue;
            }
        }
        if (i + 1 < buf_len) buf[i++] = l->src[l->pos];
        l->pos++;
    }
    buf[i] = '\0';
    if (l->pos >= l->len || l->src[l->pos] != '"') return -1;
    l->pos++;
    return 0;
}

/** 词法 peek：取下一个标识符但不消耗 */
static int lex_peek_ident(yang_lexer_t *l, char *buf, size_t buf_len) {
    size_t saved = l->pos;
    if (!lex_ident(l, buf, buf_len)) return 0;
    l->pos = saved;
    return 1;
}

/* ============================================================
 * 模型生命周期
 * ============================================================ */

yang_schema_node_t *yang_schema_node_create(const char *name,
                                            yang_schema_kind_t kind) {
    yang_schema_node_t *n = (yang_schema_node_t *)calloc(1, sizeof(yang_schema_node_t));
    if (!n) return NULL;
    SET_STR(n->name, name);
    n->kind = kind;
    n->value_type = YANG_TYPE_EMPTY;
    n->default_value[0] = '\0';
    n->description[0] = '\0';
    n->reference[0] = '\0';
    n->mandatory = false;
    n->config = true;
    return n;
}

void yang_schema_node_free(yang_schema_node_t *node) {
    if (!node) return;
    yang_schema_node_t *c = node->first_child;
    while (c) {
        yang_schema_node_t *next = c->next_sibling;
        yang_schema_node_free(c);
        c = next;
    }
    free(node);
}

int yang_schema_add_child(yang_schema_node_t *parent,
                          yang_schema_node_t *child) {
    if (!parent || !child) return -1;
    if (child->parent || child->next_sibling) return -1;
    child->parent = parent;
    child->next_sibling = NULL;
    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        yang_schema_node_t *tail = parent->first_child;
        while (tail->next_sibling) tail = tail->next_sibling;
        tail->next_sibling = child;
    }
    return 0;
}

yang_model_t *yang_model_create(const char *module_name) {
    yang_model_t *m = (yang_model_t *)calloc(1, sizeof(yang_model_t));
    if (!m) return NULL;
    SET_STR(m->module_name, module_name);
    m->ns_uri[0] = '\0';
    m->root = yang_schema_node_create(module_name, YANG_SCHEMA_MODULE);
    if (!m->root) {
        free(m);
        return NULL;
    }
    m->node_count = 1;
    return m;
}

void yang_model_free(yang_model_t *model) {
    if (!model) return;
    yang_schema_node_free(model->root);
    free(model);
}

/* ============================================================
 * 递归下降解析
 * ============================================================ */

/* 前向声明 */
static int parse_stmt_block(yang_lexer_t *l, yang_schema_node_t *parent);

static int parse_leaf_inner(yang_lexer_t *l, yang_schema_node_t *node) {
    if (lex_expect(l, "type") != 0) return -1;
    char type_name[YANG_MAX_NAME_LEN];
    if (!lex_ident(l, type_name, sizeof(type_name))) return -1;
    node->value_type = yang_data_type_from_string(type_name);
    if (lex_expect(l, ";") != 0) return -1;
    return 0;
}

static int parse_leaf(yang_lexer_t *l, yang_schema_node_t *parent) {
    char name[YANG_MAX_NAME_LEN];
    if (!lex_ident(l, name, sizeof(name))) return -1;
    if (lex_punct(l, '{') != 0) return -1;

    yang_schema_node_t *node = yang_schema_node_create(name, YANG_SCHEMA_LEAF);
    if (!node) return -1;

    /* 解析子语句直到 '}' */
    while (1) {
        skip_ws_and_comments(l);
        if (l->pos < l->len && l->src[l->pos] == '}') { l->pos++; break; }
        char kw[YANG_MAX_NAME_LEN];
        if (!lex_ident(l, kw, sizeof(kw))) {
            yang_schema_node_free(node);
            return -1;
        }
        if (strcmp(kw, "type") == 0) {
            char t[YANG_MAX_NAME_LEN];
            if (!lex_ident(l, t, sizeof(t))) goto fail;
            node->value_type = yang_data_type_from_string(t);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "default") == 0) {
            char buf[YANG_MAX_VALUE_LEN];
            if (lex_quoted(l, buf, sizeof(buf)) != 0) goto fail;
            SET_STR(node->default_value, buf);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "mandatory") == 0) {
            char val[YANG_MAX_NAME_LEN];
            if (!lex_ident(l, val, sizeof(val))) goto fail;
            node->mandatory = (strcmp(val, "true") == 0);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "config") == 0) {
            char val[YANG_MAX_NAME_LEN];
            if (!lex_ident(l, val, sizeof(val))) goto fail;
            node->config = (strcmp(val, "true") == 0);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "description") == 0) {
            char buf[512];
            if (lex_quoted(l, buf, sizeof(buf)) != 0) goto fail;
            SET_STR(node->description, buf);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "reference") == 0) {
            char buf[256];
            if (lex_quoted(l, buf, sizeof(buf)) != 0) goto fail;
            SET_STR(node->reference, buf);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "must") == 0 || strcmp(kw, "when") == 0) {
            char buf[YANG_MAX_VALUE_LEN];
            if (lex_quoted(l, buf, sizeof(buf)) != 0) goto fail;
            if (lex_punct(l, ';') != 0) goto fail;
        } else {
            /* 未知子语句：尝试作为无值语句（如 'units' 等），需 ';' 收尾 */
            if (lex_peek_ident(l, kw, sizeof(kw))) {
                char dummy[YANG_MAX_NAME_LEN];
                if (!lex_ident(l, dummy, sizeof(dummy))) goto fail;
            }
            if (lex_punct(l, ';') != 0) goto fail;
        }
    }

    yang_schema_add_child(parent, node);
    return 0;
fail:
    yang_schema_node_free(node);
    return -1;
}

static int parse_list(yang_lexer_t *l, yang_schema_node_t *parent) {
    char name[YANG_MAX_NAME_LEN];
    if (!lex_ident(l, name, sizeof(name))) return -1;
    if (lex_punct(l, '{') != 0) return -1;

    yang_schema_node_t *node = yang_schema_node_create(name, YANG_SCHEMA_LIST);
    if (!node) return -1;

    while (1) {
        skip_ws_and_comments(l);
        if (l->pos < l->len && l->src[l->pos] == '}') { l->pos++; break; }
        char kw[YANG_MAX_NAME_LEN];
        if (!lex_ident(l, kw, sizeof(kw))) goto fail;
        if (strcmp(kw, "key") == 0) {
            char buf[256];
            if (lex_quoted(l, buf, sizeof(buf)) != 0) goto fail;
            /* 空格分隔的键名 */
            char *save = NULL;
            char *tok = strtok_r(buf, " ", &save);
            int k = 0;
            while (tok && k < 8) {
                SET_STR(node->keys[k], tok);
                k++;
                tok = strtok_r(NULL, " ", &save);
            }
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "config") == 0) {
            char val[YANG_MAX_NAME_LEN];
            if (!lex_ident(l, val, sizeof(val))) goto fail;
            node->config = (strcmp(val, "true") == 0);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "description") == 0) {
            char buf[512];
            if (lex_quoted(l, buf, sizeof(buf)) != 0) goto fail;
            SET_STR(node->description, buf);
            if (lex_punct(l, ';') != 0) goto fail;
        } else {
            /* 透传：当作子块语句 */
            l->pos -= strlen(kw);
            if (parse_stmt_block(l, node) != 0) goto fail;
        }
    }

    yang_schema_add_child(parent, node);
    return 0;
fail:
    yang_schema_node_free(node);
    return -1;
}

static int parse_container(yang_lexer_t *l, yang_schema_node_t *parent) {
    char name[YANG_MAX_NAME_LEN];
    if (!lex_ident(l, name, sizeof(name))) return -1;
    if (lex_punct(l, '{') != 0) return -1;

    yang_schema_node_t *node = yang_schema_node_create(name, YANG_SCHEMA_CONTAINER);
    if (!node) return -1;

    while (1) {
        skip_ws_and_comments(l);
        if (l->pos < l->len && l->src[l->pos] == '}') { l->pos++; break; }
        char kw[YANG_MAX_NAME_LEN];
        if (!lex_ident(l, kw, sizeof(kw))) goto fail;
        if (strcmp(kw, "config") == 0) {
            char val[YANG_MAX_NAME_LEN];
            if (!lex_ident(l, val, sizeof(val))) goto fail;
            node->config = (strcmp(val, "true") == 0);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "description") == 0) {
            char buf[512];
            if (lex_quoted(l, buf, sizeof(buf)) != 0) goto fail;
            SET_STR(node->description, buf);
            if (lex_punct(l, ';') != 0) goto fail;
        } else {
            l->pos -= strlen(kw);
            if (parse_stmt_block(l, node) != 0) goto fail;
        }
    }

    yang_schema_add_child(parent, node);
    return 0;
fail:
    yang_schema_node_free(node);
    return -1;
}

static int parse_leaf_list(yang_lexer_t *l, yang_schema_node_t *parent) {
    char name[YANG_MAX_NAME_LEN];
    if (!lex_ident(l, name, sizeof(name))) return -1;
    if (lex_punct(l, '{') != 0) return -1;

    yang_schema_node_t *node = yang_schema_node_create(name, YANG_SCHEMA_LEAF_LIST);
    if (!node) return -1;

    while (1) {
        skip_ws_and_comments(l);
        if (l->pos < l->len && l->src[l->pos] == '}') { l->pos++; break; }
        char kw[YANG_MAX_NAME_LEN];
        if (!lex_ident(l, kw, sizeof(kw))) goto fail;
        if (strcmp(kw, "type") == 0) {
            char t[YANG_MAX_NAME_LEN];
            if (!lex_ident(l, t, sizeof(t))) goto fail;
            node->value_type = yang_data_type_from_string(t);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "config") == 0) {
            char val[YANG_MAX_NAME_LEN];
            if (!lex_ident(l, val, sizeof(val))) goto fail;
            node->config = (strcmp(val, "true") == 0);
            if (lex_punct(l, ';') != 0) goto fail;
        } else if (strcmp(kw, "description") == 0) {
            char buf[512];
            if (lex_quoted(l, buf, sizeof(buf)) != 0) goto fail;
            SET_STR(node->description, buf);
            if (lex_punct(l, ';') != 0) goto fail;
        } else {
            char dummy[YANG_MAX_NAME_LEN];
            if (!lex_ident(l, dummy, sizeof(dummy))) goto fail;
            if (lex_punct(l, ';') != 0) goto fail;
        }
    }

    yang_schema_add_child(parent, node);
    return 0;
fail:
    yang_schema_node_free(node);
    return -1;
}

/** 解析一个语句（按需递归子块） */
static int parse_stmt_block(yang_lexer_t *l, yang_schema_node_t *parent) {
    char kw[YANG_MAX_NAME_LEN];
    if (!lex_ident(l, kw, sizeof(kw))) return -1;

    if (strcmp(kw, "container") == 0) return parse_container(l, parent);
    if (strcmp(kw, "leaf") == 0) return parse_leaf(l, parent);
    if (strcmp(kw, "leaf-list") == 0) return parse_leaf_list(l, parent);
    if (strcmp(kw, "list") == 0) return parse_list(l, parent);
    if (strcmp(kw, "typedef") == 0) {
        /* 跳过 typedef 内容（不展开，简化） */
        if (!lex_ident(l, kw, sizeof(kw))) return -1;
        if (lex_punct(l, '{') != 0) return -1;
        int depth = 1;
        while (depth > 0 && l->pos < l->len) {
            char c = l->src[l->pos];
            if (c == '{') depth++;
            else if (c == '}') depth--;
            l->pos++;
        }
        return 0;
    }
    if (strcmp(kw, "grouping") == 0) {
        if (!lex_ident(l, kw, sizeof(kw))) return -1;
        if (lex_punct(l, '{') != 0) return -1;
        int depth = 1;
        while (depth > 0 && l->pos < l->len) {
            char c = l->src[l->pos];
            if (c == '{') depth++;
            else if (c == '}') depth--;
            l->pos++;
        }
        return 0;
    }
    if (strcmp(kw, "uses") == 0) {
        char name[YANG_MAX_NAME_LEN];
        if (!lex_ident(l, name, sizeof(name))) return -1;
        return lex_punct(l, ';');
    }
    /* 未知语句：尝试跳过 (id 或 id value) ';' */
    char next[YANG_MAX_NAME_LEN];
    if (lex_peek_ident(l, next, sizeof(next))) {
        if (!lex_ident(l, next, sizeof(next))) return -1;
    }
    return lex_punct(l, ';');
}

int yang_model_parse(yang_model_t *model, const char *text, size_t len) {
    if (!model || !text) return -1;
    yang_lexer_t l;
    lex_init(&l, text, len);

    /* 期望 module name { ... } */
    char kw[YANG_MAX_NAME_LEN];
    if (!lex_ident(&l, kw, sizeof(kw))) return -1;
    if (strcmp(kw, "module") != 0) return -1;

    char name[YANG_MAX_NAME_LEN];
    if (!lex_ident(&l, name, sizeof(name))) return -1;
    SET_STR(model->module_name, name);

    /* 模块根节点名 = module 名 */
    SET_STR(model->root->name, name);

    if (lex_punct(&l, '{') != 0) return -1;

    while (1) {
        skip_ws_and_comments(&l);
        if (l.pos < l.len && l.src[l.pos] == '}') { l.pos++; break; }
        if (parse_stmt_block(&l, model->root) != 0) return -1;
    }

    /* 重新统计节点数 */
    size_t cnt = 0;
    yang_schema_node_t *stack[2048];
    int sp = 0;
    stack[sp++] = model->root;
    while (sp > 0) {
        yang_schema_node_t *n = stack[--sp];
        cnt++;
        for (yang_schema_node_t *c = n->first_child; c; c = c->next_sibling) {
            if (sp < 2048) stack[sp++] = c;
        }
    }
    model->node_count = cnt;
    return 0;
}

yang_model_t *yang_model_load_file(const char *path) {
    if (!path) return NULL;
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) { fclose(fp); return NULL; }

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t n = fread(buf, 1, (size_t)size, fp);
    buf[n] = '\0';
    fclose(fp);

    yang_model_t *m = yang_model_create("module");
    if (!m) { free(buf); return NULL; }

    if (yang_model_parse(m, buf, n) != 0) {
        yang_model_free(m);
        free(buf);
        return NULL;
    }
    free(buf);
    return m;
}

yang_schema_node_t *yang_schema_find(yang_schema_node_t *root,
                                     const char *path) {
    if (!root || !path) return NULL;
    while (*path == '/') path++;
    if (*path == '\0') return root;

    yang_schema_node_t *current = root;

    /* 若路径首段等于 root 名称，则跳过 root 直接进入子树 */
    const char *slash = strchr(path, '/');
    if (slash) {
        size_t first_len = (size_t)(slash - path);
        char first[YANG_MAX_NAME_LEN];
        if (first_len > 0 && first_len < sizeof(first)) {
            memcpy(first, path, first_len);
            first[first_len] = '\0';
            if (strcmp(first, current->name) == 0) {
                path = slash + 1;
                while (*path == '/') path++;
            }
        }
    } else if (strcmp(path, current->name) == 0) {
        return current;
    }

    char name[YANG_MAX_NAME_LEN];
    while (*path) {
        const char *p = path;
        size_t i = 0;
        while (*p && *p != '/') {
            if (i + 1 < sizeof(name)) name[i++] = *p;
            p++;
        }
        name[i] = '\0';
        yang_schema_node_t *next = NULL;
        for (yang_schema_node_t *c = current->first_child; c; c = c->next_sibling) {
            if (strcmp(c->name, name) == 0) { next = c; break; }
        }
        if (!next) return NULL;
        current = next;
        path = p;
        while (*path == '/') path++;
    }
    return current;
}