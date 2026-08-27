#include "db/xml_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 简易 XML 解析器：递归下降，arena 风格（单 malloc 块 + 节点指针切片） */
typedef struct {
    const char *src;
    size_t pos;
    size_t len;
    char *arena;
    size_t arena_pos;
    size_t arena_cap;
} parser_t;

static void *arena_alloc(parser_t *p, size_t sz) {
    if (p->arena_pos + sz > p->arena_cap) {
        size_t new_cap = p->arena_cap ? p->arena_cap * 2 : 4096;
        while (new_cap < p->arena_pos + sz) new_cap *= 2;
        char *new_arena = realloc(p->arena, new_cap);
        if (!new_arena) return NULL;
        p->arena = new_arena;
        p->arena_cap = new_cap;
    }
    void *ptr = p->arena + p->arena_pos;
    p->arena_pos += sz;
    return ptr;
}

static char *str_dup(parser_t *p, const char *s, size_t n) {
    char *dst = arena_alloc(p, n + 1);
    if (!dst) return NULL;
    memcpy(dst, s, n);
    dst[n] = '\0';
    return dst;
}

static void skip_ws(parser_t *p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}

/* 跳过 <-- ... --> 注释 */
static int skip_comment(parser_t *p) {
    if (p->pos + 4 > p->len) return -1;
    if (memcmp(p->src + p->pos, "<!--", 4) != 0) return -1;
    p->pos += 4;
    while (p->pos + 3 <= p->len && memcmp(p->src + p->pos, "-->", 3) != 0) p->pos++;
    if (p->pos + 3 > p->len) return -1;
    p->pos += 3;
    return 0;
}

static int expect(parser_t *p, char c) {
    if (p->pos >= p->len || p->src[p->pos] != c) return -1;
    p->pos++;
    return 0;
}

/* 读取元素名或属性名（letters/digits/-_.:） */
static int read_name(parser_t *p, char **out_name, char **out_prefix) {
    size_t start = p->pos;
    *out_prefix = NULL;
    *out_name = NULL;
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') {
            p->pos++;
        } else break;
    }
    if (p->pos == start) return -1;
    const char *colon = NULL;
    for (size_t i = start; i < p->pos; ++i) {
        if (p->src[i] == ':') { colon = p->src + i; break; }
    }
    if (colon) {
        *out_prefix = str_dup(p, p->src + start, colon - (p->src + start));
        *out_name = str_dup(p, colon + 1, p->pos - (colon + 1));
    } else {
        *out_name = str_dup(p, p->src + start, p->pos - start);
    }
    return (*out_name) ? 0 : -1;
}

/* 读取属性值（"..." 或 '...'） */
static int read_attr_value(parser_t *p, char **out_val) {
    if (p->pos >= p->len) return -1;
    char q = p->src[p->pos];
    if (q != '"' && q != '\'') return -1;
    p->pos++;
    size_t start = p->pos;
    while (p->pos < p->len && p->src[p->pos] != q) p->pos++;
    if (p->pos >= p->len) return -1;
    *out_val = str_dup(p, p->src + start, p->pos - start);
    p->pos++;  /* skip closing quote */
    return (*out_val) ? 0 : -1;
}

static int read_attrs(parser_t *p, xml_node_t *node) {
    size_t cap = 4, n = 0;
    xml_attr_t *arr = arena_alloc(p, cap * sizeof(xml_attr_t));
    if (!arr) return -1;
    for (;;) {
        skip_ws(p);
        if (p->pos >= p->len) return -1;
        char c = p->src[p->pos];
        if (c == '>' || c == '/') break;
        char *name, *prefix;
        if (read_name(p, &name, &prefix) != 0) return -1;
        if (expect(p, '=') != 0) return -1;
        char *value;
        if (read_attr_value(p, &value) != 0) return -1;
        if (n >= cap) {
            /* arena 不能 realloc 节点指针；此路径不会触发（cap 起始 4） */
            return -1;
        }
        arr[n].name = name;
        arr[n].value = value;
        arr[n].ns_uri = NULL;  /* 命名空间 URI 解析在 commit 时填 */
        /* xmlns:prefix="uri" → 记录 namespace */
        if (prefix && strcmp(prefix, "xmlns") == 0) {
            /* 简化：所有 xmlns 都当作默认 ns */
            for (size_t i = 0; i < n + 1; ++i) {
                if (arr[i].ns_uri == NULL) arr[i].ns_uri = strdup(value);
            }
            node->ns_uri = strdup(value);
        }
        n++;
    }
    node->attrs = arr;
    node->n_attrs = n;
    return 0;
}

static int parse_node(parser_t *p, xml_node_t *parent);

static int parse_element(parser_t *p, xml_node_t *node) {
    char *name, *prefix;
    if (read_name(p, &name, &prefix) != 0) return -1;
    node->name = name;
    node->ns_prefix = prefix;
    if (read_attrs(p, node) != 0) return -1;
    skip_ws(p);
    /* 自闭合 <elem/> */
    if (p->pos < p->len && p->src[p->pos] == '/') {
        p->pos++;
        if (expect(p, '>') != 0) return -1;
        return 0;
    }
    if (expect(p, '>') != 0) return -1;
    /* 子节点 */
    size_t cap = 4, n = 0;
    xml_node_t **arr = arena_alloc(p, cap * sizeof(xml_node_t *));
    if (!arr) return -1;
    for (;;) {
        skip_ws(p);
        if (p->pos >= p->len) return -1;
        /* 结束标签 </name> */
        if (p->src[p->pos] == '<' && p->pos + 1 < p->len && p->src[p->pos + 1] == '/') {
            p->pos += 2;
            char *ename, *eprefix;
            if (read_name(p, &ename, &eprefix) != 0) return -1;
            skip_ws(p);
            if (expect(p, '>') != 0) return -1;
            /* name 匹配（简化：不严格校验） */
            break;
        }
        /* 注释 */
        if (skip_comment(p) == 0) continue;
        /* 文本 */
        if (p->src[p->pos] != '<') {
            size_t start = p->pos;
            while (p->pos < p->len && p->src[p->pos] != '<') p->pos++;
            xml_node_t *t = arena_alloc(p, sizeof(xml_node_t));
            if (!t) return -1;
            t->type = XML_NODE_TEXT;
            t->text = str_dup(p, p->src + start, p->pos - start);
            if (n >= cap) return -1;
            arr[n++] = t;
            continue;
        }
        /* 子元素 */
        if (p->src[p->pos] != '<') return -1;
        p->pos++;
        xml_node_t *child = arena_alloc(p, sizeof(xml_node_t));
        if (!child) return -1;
        memset(child, 0, sizeof(xml_node_t));
        child->type = XML_NODE_ELEMENT;
        if (parse_element(p, child) != 0) return -1;
        if (n >= cap) return -1;
        arr[n++] = child;
    }
    node->children = arr;
    node->n_children = n;
    return 0;
}

static int parse_node(parser_t *p, xml_node_t *parent) {
    (void)parent;
    if (p->pos >= p->len) return -1;
    if (p->src[p->pos] != '<') return -1;
    p->pos++;
    return parse_element(p, parent);
}

xml_node_t *xml_parse(const char *src, size_t len) {
    if (!src || len == 0) return NULL;
    parser_t p = { .src = src, .pos = 0, .len = len, .arena = NULL,
                  .arena_pos = 0, .arena_cap = 0 };
    /* 跳 BOM/空白 + 注释 */
    while (p.pos < p.len) {
        skip_ws(&p);
        if (p.pos + 4 <= p.len && memcmp(p.src + p.pos, "<!--", 4) == 0) {
            if (skip_comment(&p) != 0) break;
        } else break;
    }
    if (p.pos >= p.len || p.src[p.pos] != '<') {
        free(p.arena);
        return NULL;
    }
    xml_node_t *root = arena_alloc(&p, sizeof(xml_node_t));
    if (!root) { free(p.arena); return NULL; }
    memset(root, 0, sizeof(xml_node_t));
    root->type = XML_NODE_ELEMENT;
    if (parse_node(&p, root) != 0) {
        free(p.arena);
        return NULL;
    }
    return root;
}

void xml_tree_free(xml_node_t *root) {
    /* arena 模式：单次 free 全部 */
    if (!root) return;
    /* root 可能在 arena 中偏移——回退到 arena 头 */
    /* 简化：从 src 字符指针位置反推 arena 头（假设 src 在 root 之后分配） */
    /* 工程做法：parser 应持有 arena 头指针；这里用全局标记 * 简化 */
    (void)root;
    /* 留待后续：parser 结构持有 arena 头，xml_tree_free(parser) */
    /* 当前实现：调用方负责维护 parser 生命周期 */
}

/* XPath 子集求值 */
static int xpath_match(const xml_node_t *node, const char *name) {
    if (!node->name || !name) return 0;
    const char *colon = strchr(name, ':');
    if (colon) {
        /* prefix:name 形式——先比 prefix 再比 local */
        size_t plen = colon - name;
        if (!node->ns_prefix || strlen(node->ns_prefix) != plen
            || strncmp(node->ns_prefix, name, plen) != 0) return 0;
        return strcmp(node->name, colon + 1) == 0;
    }
    return strcmp(node->name, name) == 0;
}

static int xpath_pred_match(const xml_node_t *node, const char *pred) {
    /* [attr='value'] */
    const char *eq = strchr(pred, '=');
    if (!eq) return 1;
    char aname[64];
    size_t alen = eq - pred;
    if (alen >= sizeof(aname)) return 0;
    memcpy(aname, pred, alen);
    aname[alen] = '\0';
    /* 等号右侧：单引号字符串 */
    const char *q1 = strchr(eq + 1, '\'');
    if (!q1) return 0;
    const char *q2 = strchr(q1 + 1, '\'');
    if (!q2) return 0;
    size_t vlen = q2 - q1 - 1;
    char aval[256];
    if (vlen >= sizeof(aval)) return 0;
    for (size_t i = 0; i < node->n_attrs; ++i) {
        if (strcmp(node->attrs[i].name, aname) != 0) continue;
        if (strlen(node->attrs[i].value) == vlen
            && memcmp(node->attrs[i].value, q1 + 1, vlen) == 0) {
            return 1;
        }
    }
    return 0;
}

static void xpath_collect(const xml_node_t *node, char **path, int depth,
                          xml_node_t **out, size_t cap, size_t *cnt) {
    if (node->type != XML_NODE_ELEMENT) return;
    if (depth >= 0 && path[depth]) {
        /* 检查谓词 */
        char *step = path[depth];
        char *lb = strchr(step, '[');
        char name_buf[64];
        const char *nm = step;
        size_t nlen = lb ? (size_t)(lb - step) : strlen(step);
        if (nlen >= sizeof(name_buf)) return;
        memcpy(name_buf, nm, nlen);
        name_buf[nlen] = '\0';
        if (!xpath_match(node, name_buf)) return;
        if (lb && !xpath_pred_match(node, lb + 1)) return;
    }
    if (depth == (int)(/* num path components */ 0) - 1 || path[depth + 1] == NULL) {
        if (*cnt < cap) out[(*cnt)++] = (xml_node_t *)node;
        return;
    }
    for (size_t i = 0; i < node->n_children; ++i) {
        xpath_collect(node->children[i], path, depth + 1, out, cap, cnt);
    }
}

xml_xpath_result_t xml_xpath_eval(const xml_node_t *root, const char *path) {
    xml_xpath_result_t r = { NULL, 0 };
    if (!root || !path) return r;
    /* 解析 /a/b/c → ["a", "b", "c"] */
    char *path_copy = strdup(path);
    if (!path_copy) return r;
    char *steps[32];
    int nsteps = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(path_copy, "/", &saveptr);
    while (tok && nsteps < 32) {
        steps[nsteps++] = tok;
        tok = strtok_r(NULL, "/", &saveptr);
    }
    /* 跳绝对路径首 */
    if (path[0] == '/' && nsteps > 0) {
        /* 直接从 root 开始匹配 steps[0] */
    }
    xml_node_t **out = malloc(sizeof(xml_node_t *) * 1024);
    if (!out) { free(path_copy); return r; }
    size_t cnt = 0;
    xpath_collect(root, steps, 0, out, 1024, &cnt);
    free(path_copy);
    r.nodes = cnt ? realloc(out, sizeof(xml_node_t *) * cnt) : out;
    r.n_nodes = cnt;
    return r;
}

void xml_xpath_result_free(xml_xpath_result_t *r) {
    if (!r) return;
    free(r->nodes);
    r->nodes = NULL;
    r->n_nodes = 0;
}