/**
 * @file netconf_server.c
 * @brief NETCONF 1.0 协议服务器实现
 *
 * 提供内存态 NETCONF 会话、RPC 处理和 XML 序列化。
 * 本实现为简化版：使用字符串扫描做 XML 解析，不支持 XML 属性、命名空间前缀。
 */
#include "db/netconf/netconf_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================
 * 辅助宏与字符串工具
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

/** 返回串中下一个标记的开始位置（跳过空白与 '<'） */
static const char *skip_ws_lt(const char *p) {
    if (!p) return NULL;
    while (*p && (isspace((unsigned char)*p) || *p == '<')) p++;
    return p;
}

/** 提取一个标识符（字母/数字/'-'/'_'/'.'），返回末尾位置 */
static const char *read_ident(const char *p, char *out, size_t out_len) {
    size_t i = 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == ':' || *p == '.')) {
        if (i + 1 < out_len) out[i++] = *p;
        p++;
    }
    out[i] = '\0';
    return p;
}

/* ============================================================
 * XML 序列化
 * ============================================================ */

static int serialize_node(const yang_data_node_t *node,
                          char *buf, size_t buf_size, size_t *used) {
    if (!node || !used) return -1;
    if (*used + 256 > buf_size) return -1;

    /* 跳过 root 层：仅序列化子内容 */
    if (node->kind == YANG_KIND_CONTAINER && node->parent == NULL) {
        for (yang_data_node_t *c = node->first_child; c; c = c->next_sibling) {
            if (serialize_node(c, buf, buf_size, used) != 0) return -1;
        }
        return 0;
    }

    int n = snprintf(buf + *used, buf_size - *used, "<%s", node->name);
    if (n < 0 || (size_t)n >= buf_size - *used) return -1;
    *used += (size_t)n;

    /* list：附加 key="value" 属性 */
    if (node->kind == YANG_KIND_LIST) {
        for (int i = 0; i < 8 && node->keys[i][0]; i++) {
            yang_data_node_t *kv = yang_data_find_child((yang_data_node_t *)node,
                                                         node->keys[i]);
            const char *v = kv ? kv->value : "";
            n = snprintf(buf + *used, buf_size - *used,
                         " %s=\"%s\"", node->keys[i], v);
            if (n < 0 || (size_t)n >= buf_size - *used) return -1;
            *used += (size_t)n;
        }
    }

    if (!node->first_child && (!node->value || node->value[0] == '\0')) {
        n = snprintf(buf + *used, buf_size - *used, "/>");
        if (n < 0 || (size_t)n >= buf_size - *used) return -1;
        *used += (size_t)n;
        return 0;
    }

    n = snprintf(buf + *used, buf_size - *used, ">");
    if (n < 0 || (size_t)n >= buf_size - *used) return -1;
    *used += (size_t)n;

    if (node->first_child) {
        for (yang_data_node_t *c = node->first_child; c; c = c->next_sibling) {
            if (serialize_node(c, buf, buf_size, used) != 0) return -1;
        }
    } else if (node->value[0]) {
        /* 转义简单特殊字符 */
        for (const char *p = node->value; *p; p++) {
            if (*used + 8 >= buf_size) return -1;
            const char *repl = NULL;
            switch (*p) {
                case '<': repl = "&lt;"; break;
                case '>': repl = "&gt;"; break;
                case '&': repl = "&amp;"; break;
                case '"': repl = "&quot;"; break;
                case '\'': repl = "&apos;"; break;
            }
            if (repl) {
                size_t rlen = strlen(repl);
                if (*used + rlen >= buf_size) return -1;
                memcpy(buf + *used, repl, rlen);
                *used += rlen;
            } else {
                buf[(*used)++] = *p;
            }
        }
    }

    n = snprintf(buf + *used, buf_size - *used, "</%s>", node->name);
    if (n < 0 || (size_t)n >= buf_size - *used) return -1;
    *used += (size_t)n;
    return 0;
}

int netconf_xml_serialize(const yang_data_node_t *root,
                          char *buf, size_t buf_size) {
    if (!root || !buf || buf_size == 0) return -1;
    buf[0] = '\0';
    size_t used = 0;
    if (serialize_node(root, buf, buf_size, &used) != 0) return -1;
    return (int)used;
}

/* ============================================================
 * XML 简易解析
 * ============================================================ */

/**
 * @brief 在 buf 中寻找 "<name>" 或 "<name " 开始的元素起始。
 * 返回指向 '<' 之后（即标识符起始）的位置。
 */
static const char *find_elem_start(const char *p, const char *name) {
    char tag[YANG_MAX_NAME_LEN];
    while (*p) {
        const char *lt = strchr(p, '<');
        if (!lt) return NULL;
        const char *next = lt + 1;
        if (*next == '/' || *next == '?') { p = lt + 1; continue; }
        const char *end = read_ident(next, tag, sizeof(tag));
        if (strcmp(tag, name) == 0) return next;
        p = lt + 1;
    }
    return NULL;
}

/**
 * @brief 在 buf 中找到 "<name>" 或 "<name " 的元素，提取内容到 out。
 *
 * 简单起见：定位 "<name" 起始后，跳过属性直到 '>' 或 "/>"，
 * - 若 "/>" 则视为空元素，out 写入空串；
 * - 若 ">" 则读取直到 "</name>"，提取内部文本到 out。
 *
 * @return 成功时返回 '<' 之前的位置；找不到返回 NULL。
 */
const char *netconf_xml_find(const char *buf, const char *name,
                             char *out, size_t out_size) {
    if (!buf || !name || !out || out_size == 0) return NULL;
    out[0] = '\0';

    const char *p = buf;
    while (1) {
        const char *lt = strchr(p, '<');
        if (!lt) return NULL;
        if (lt[1] == '/' || lt[1] == '?') { p = lt + 1; continue; }
        char tag[YANG_MAX_NAME_LEN];
        const char *after_name = read_ident(lt + 1, tag, sizeof(tag));
        if (strcmp(tag, name) != 0) {
            p = lt + 1;
            continue;
        }
        /* 找到目标元素开始；扫描属性直到 '>' */
        const char *q = after_name;
        while (*q && *q != '>' && *q != '/') q++;
        if (*q == '/') {
            /* 自闭合空元素 */
            return lt + 1;
        }
        if (*q != '>') return NULL;
        const char *content = q + 1;

        /* 找结束标签 </name> */
        char endtag[YANG_MAX_NAME_LEN + 4];
        snprintf(endtag, sizeof(endtag), "</%s>", name);
        const char *end = strstr(content, endtag);
        if (!end) return NULL;
        size_t len = (size_t)(end - content);
        if (len + 1 > out_size) len = out_size - 1;
        memcpy(out, content, len);
        out[len] = '\0';
        return content;
    }
}

/**
 * @brief 解析子树：把 <config>...</config> 内的子节点 name/value 提取出来。
 *
 * 简易策略：扫描 <config>...</config> 中的所有开标签，记下名，
 * 并对叶子节点取 '>' 与 '</name>' 之间的文本。
 */
int netconf_xml_parse_subtree(const char *buf,
                              char (*out_names)[YANG_MAX_NAME_LEN],
                              char (*out_values)[YANG_MAX_VALUE_LEN],
                              int max_count) {
    if (!buf || !out_names || !out_values || max_count <= 0) return 0;
    int count = 0;
    const char *p = buf;
    while (*p && count < max_count) {
        const char *lt = strchr(p, '<');
        if (!lt) break;
        if (lt[1] == '/' || lt[1] == '?') { p = lt + 1; continue; }
        char tag[YANG_MAX_NAME_LEN];
        const char *after = read_ident(lt + 1, tag, sizeof(tag));
        /* 跳过属性直到 '>' 或 '/' */
        const char *q = after;
        while (*q && *q != '>' && *q != '/') q++;
        if (*q == '/') {
            /* 自闭合，叶子节点；记录空值 */
            SET_STR(out_names[count], tag);
            out_values[count][0] = '\0';
            count++;
            p = q + 2;
            continue;
        }
        if (*q != '>') break;
        const char *content = q + 1;
        char endtag[YANG_MAX_NAME_LEN + 4];
        snprintf(endtag, sizeof(endtag), "</%s>", tag);
        const char *end = strstr(content, endtag);
        if (!end) break;

        /* 看是否含有子标签（无则为叶子节点） */
        const char *inner_lt = strchr(content, '<');
        if (!inner_lt || inner_lt >= end) {
            /* 叶子节点 */
            SET_STR(out_names[count], tag);
            size_t len = (size_t)(end - content);
            if (len + 1 > YANG_MAX_VALUE_LEN) len = YANG_MAX_VALUE_LEN - 1;
            memcpy(out_values[count], content, len);
            out_values[count][len] = '\0';
            count++;
        }
        p = end + strlen(endtag);
    }
    return count;
}

/* ============================================================
 * 会话生命周期
 * ============================================================ */

netconf_session_t *netconf_session_create(const char *session_id) {
    netconf_session_t *s = (netconf_session_t *)calloc(1, sizeof(netconf_session_t));
    if (!s) return NULL;
    SET_STR(s->session_id, session_id);
    s->running = yang_datastore_create(NETCONF_DS_RUNNING);
    s->startup = yang_datastore_create(NETCONF_DS_STARTUP);
    s->candidate = yang_datastore_create(NETCONF_DS_CANDIDATE);
    s->model = NULL;
    s->message_id = 0;
    s->locked = false;
    if (!s->running || !s->startup || !s->candidate) {
        netconf_session_free(s);
        return NULL;
    }
    return s;
}

void netconf_session_free(netconf_session_t *s) {
    if (!s) return;
    if (s->running) yang_datastore_free(s->running);
    if (s->startup) yang_datastore_free(s->startup);
    if (s->candidate) yang_datastore_free(s->candidate);
    free(s);
}

void netconf_session_set_model(netconf_session_t *s, yang_model_t *model) {
    if (!s) return;
    s->model = model;
}

yang_datastore_t *netconf_session_get_datastore(netconf_session_t *s,
                                                const char *name) {
    if (!s || !name) return NULL;
    if (strcmp(name, NETCONF_DS_RUNNING) == 0) return s->running;
    if (strcmp(name, NETCONF_DS_STARTUP) == 0) return s->startup;
    if (strcmp(name, NETCONF_DS_CANDIDATE) == 0) return s->candidate;
    return NULL;
}

/* ============================================================
 * RPC 实现
 * ============================================================ */

static netconf_result_t write_ok_reply(const char *msg_id,
                                       char *out, size_t out_size) {
    int n = snprintf(out, out_size,
        "<rpc-reply xmlns=\"%s\" message-id=\"%s\">"
        "<ok/></rpc-reply>",
        NETCONF_NS, msg_id ? msg_id : "0");
    if (n < 0 || (size_t)n >= out_size) return NETCONF_ERR_INTERNAL;
    return NETCONF_OK;
}

static netconf_result_t write_data_reply(const char *msg_id,
                                         const char *data_xml,
                                         char *out, size_t out_size) {
    int n = snprintf(out, out_size,
        "<rpc-reply xmlns=\"%s\" message-id=\"%s\">"
        "<data>%s</data></rpc-reply>",
        NETCONF_NS, msg_id ? msg_id : "0",
        data_xml ? data_xml : "");
    if (n < 0 || (size_t)n >= out_size) return NETCONF_ERR_INTERNAL;
    return NETCONF_OK;
}

static const char *find_message_id(const char *rpc) {
    static char id_buf[64];
    id_buf[0] = '\0';
    const char *p = strstr(rpc, "message-id=\"");
    if (!p) return "0";
    p += strlen("message-id=\"");
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < sizeof(id_buf)) id_buf[i++] = *p++;
    id_buf[i] = '\0';
    return id_buf;
}

netconf_result_t netconf_op_get(netconf_session_t *s,
                                const yang_data_node_t *filter,
                                char *reply_out, size_t reply_size) {
    if (!s || !s->running || !reply_out) return NETCONF_ERR_INVALID_RPC;
    const char *msg_id = find_message_id(NULL);  /* 由 handle_rpc 注入 */
    (void)msg_id;

    char data_buf[NETCONF_BUF_SIZE];
    const yang_data_node_t *root = filter ? filter : s->running->root;
    int n = netconf_xml_serialize(root, data_buf, sizeof(data_buf));
    if (n < 0) return NETCONF_ERR_INTERNAL;

    return write_data_reply(NULL, data_buf, reply_out, reply_size);
}

netconf_result_t netconf_op_get_config(netconf_session_t *s,
                                       const char *source,
                                       const yang_data_node_t *filter,
                                       char *reply_out, size_t reply_size) {
    if (!s || !source || !reply_out) return NETCONF_ERR_INVALID_RPC;
    yang_datastore_t *ds = netconf_session_get_datastore(s, source);
    if (!ds) return NETCONF_ERR_DATABASE;

    char data_buf[NETCONF_BUF_SIZE];
    const yang_data_node_t *root = filter ? filter : ds->root;
    int n = netconf_xml_serialize(root, data_buf, sizeof(data_buf));
    if (n < 0) return NETCONF_ERR_INTERNAL;
    return write_data_reply(NULL, data_buf, reply_out, reply_size);
}

static netconf_result_t apply_edit(yang_datastore_t *ds,
                                   const yang_data_node_t *config,
                                   netconf_edit_op_t op) {
    if (!ds || !config) return NETCONF_ERR_INVALID_RPC;

    if (op == NETCONF_EDIT_DELETE || op == NETCONF_EDIT_REMOVE) {
        /* 删除 config 子树 */
        for (yang_data_node_t *c = config->first_child; c; c = c->next_sibling) {
            yang_data_remove_child(ds->root, c->name);
        }
        return NETCONF_OK;
    }

    if (op == NETCONF_EDIT_CREATE) {
        /* 仅在不存在时插入 */
        for (yang_data_node_t *c = config->first_child; c; c = c->next_sibling) {
            if (yang_data_find_child(ds->root, c->name)) continue;
            yang_data_node_t *clone = yang_data_clone(c);
            if (!clone) return NETCONF_ERR_INTERNAL;
            yang_data_add_child(ds->root, clone);
        }
        return NETCONF_OK;
    }

    /* MERGE / REPLACE：清空后整树替换 */
    if (op == NETCONF_EDIT_REPLACE) {
        yang_datastore_clear(ds);
    }
    for (yang_data_node_t *c = config->first_child; c; c = c->next_sibling) {
        /* 同名先移除再插入 */
        yang_data_remove_child(ds->root, c->name);
        yang_data_node_t *clone = yang_data_clone(c);
        if (!clone) return NETCONF_ERR_INTERNAL;
        yang_data_add_child(ds->root, clone);
    }
    return NETCONF_OK;
}

netconf_result_t netconf_op_edit_config(netconf_session_t *s,
                                        const char *target,
                                        const yang_data_node_t *config,
                                        netconf_edit_op_t op) {
    if (!s || !target || !config) return NETCONF_ERR_INVALID_RPC;
    yang_datastore_t *ds = netconf_session_get_datastore(s, target);
    if (!ds) return NETCONF_ERR_DATABASE;
    return apply_edit(ds, config, op);
}

netconf_result_t netconf_op_copy_config(netconf_session_t *s,
                                        const char *source,
                                        const char *target) {
    if (!s || !source || !target) return NETCONF_ERR_INVALID_RPC;
    yang_datastore_t *src = netconf_session_get_datastore(s, source);
    yang_datastore_t *dst = netconf_session_get_datastore(s, target);
    if (!src || !dst) return NETCONF_ERR_DATABASE;

    yang_datastore_clear(dst);
    for (yang_data_node_t *c = src->root->first_child; c; c = c->next_sibling) {
        yang_data_node_t *clone = yang_data_clone(c);
        if (!clone) return NETCONF_ERR_INTERNAL;
        yang_data_add_child(dst->root, clone);
    }
    return NETCONF_OK;
}

netconf_result_t netconf_op_delete_config(netconf_session_t *s,
                                          const char *target) {
    if (!s || !target) return NETCONF_ERR_INVALID_RPC;
    if (strcmp(target, NETCONF_DS_RUNNING) == 0) {
        return NETCONF_ERR_INVALID_VALUE;
    }
    return yang_datastore_clear(netconf_session_get_datastore(s, target));
}

/**
 * @brief 处理 RPC 顶层调度
 */
netconf_result_t netconf_handle_rpc(netconf_session_t *s,
                                    const char *rpc_in, size_t rpc_len,
                                    char *reply_out, size_t reply_size) {
    if (!s || !rpc_in || !reply_out) return NETCONF_ERR_INVALID_RPC;
    if (rpc_len == 0) rpc_len = strlen(rpc_in);

    (void)rpc_len;
    const char *msg_id = find_message_id(rpc_in);

    /* 简易解析：根据操作关键字调度 */
    char op_name[64];
    op_name[0] = '\0';
    /* 找 <rpc> 内的第一个子操作标签 */
    const char *p = strstr(rpc_in, "<rpc");
    if (!p) return NETCONF_ERR_INVALID_RPC;
    p = strchr(p, '>');
    if (!p) return NETCONF_ERR_INVALID_RPC;
    p++;
    while (*p && (isspace((unsigned char)*p) || *p == '<')) p++;
    const char *q = p;
    while (*q && isalnum((unsigned char)*q) || *q == '-') q++;
    size_t op_len = (size_t)(q - p);
    if (op_len == 0 || op_len + 1 > sizeof(op_name)) {
        return NETCONF_ERR_UNKNOWN_OP;
    }
    memcpy(op_name, p, op_len);
    op_name[op_len] = '\0';

    if (strcmp(op_name, "get") == 0) {
        /* <get><filter>...</filter></get> - 简化：无 filter 即全树 */
        char filter_xml[NETCONF_BUF_SIZE];
        const char *f = netconf_xml_find(rpc_in, "filter", filter_xml, sizeof(filter_xml));
        (void)f;
        char data_buf[NETCONF_BUF_SIZE];
        netconf_xml_serialize(s->running->root, data_buf, sizeof(data_buf));
        return write_data_reply(msg_id, data_buf, reply_out, reply_size);
    }

    if (strcmp(op_name, "get-config") == 0) {
        char source[64];
        const char *sp = netconf_xml_find(rpc_in, "source", source, sizeof(source));
        if (!sp) return NETCONF_ERR_INVALID_RPC;
        /* source 内容形如 <running/>，提取 datastore 名 */
        char ds_name[64];
        ds_name[0] = '\0';
        const char *r = strstr(source, "<");
        if (!r) return NETCONF_ERR_INVALID_RPC;
        r++;
        const char *re = r;
        while (*re && (isalnum((unsigned char)*re) || *re == '-')) re++;
        size_t dn_len = (size_t)(re - r);
        if (dn_len == 0 || dn_len + 1 > sizeof(ds_name)) return NETCONF_ERR_INVALID_RPC;
        memcpy(ds_name, r, dn_len);
        ds_name[dn_len] = '\0';

        yang_datastore_t *ds = netconf_session_get_datastore(s, ds_name);
        if (!ds) return NETCONF_ERR_DATABASE;
        char data_buf[NETCONF_BUF_SIZE];
        netconf_xml_serialize(ds->root, data_buf, sizeof(data_buf));
        return write_data_reply(msg_id, data_buf, reply_out, reply_size);
    }

    if (strcmp(op_name, "edit-config") == 0) {
        char target_xml[64];
        if (!netconf_xml_find(rpc_in, "target", target_xml, sizeof(target_xml))) {
            return NETCONF_ERR_INVALID_RPC;
        }
        char ds_name[64];
        ds_name[0] = '\0';
        const char *r = strchr(target_xml, '<');
        if (r) {
            r++;
            const char *re = r;
            while (*re && (isalnum((unsigned char)*re) || *re == '-')) re++;
            size_t dn_len = (size_t)(re - r);
            if (dn_len > 0 && dn_len + 1 <= sizeof(ds_name)) {
                memcpy(ds_name, r, dn_len);
                ds_name[dn_len] = '\0';
            }
        }
        if (!ds_name[0]) return NETCONF_ERR_INVALID_RPC;

        /* 解析 default-operation */
        netconf_edit_op_t op = NETCONF_EDIT_MERGE;
        char op_xml[64];
        if (netconf_xml_find(rpc_in, "default-operation", op_xml, sizeof(op_xml))) {
            if (strstr(op_xml, "replace")) op = NETCONF_EDIT_REPLACE;
            else if (strstr(op_xml, "none")) op = NETCONF_EDIT_MERGE;
        }

        /* 解析 <config> 子树 */
        char config_xml[NETCONF_BUF_SIZE];
        const char *cp = netconf_xml_find(rpc_in, "config", config_xml, sizeof(config_xml));
        if (!cp) return NETCONF_ERR_INVALID_RPC;

        char names[64][YANG_MAX_NAME_LEN];
        char values[64][YANG_MAX_VALUE_LEN];
        int count = netconf_xml_parse_subtree(config_xml, names, values, 64);

        /* 构造内存子树 */
        yang_data_node_t *cfg = yang_data_node_create("config",
                                                      YANG_KIND_CONTAINER,
                                                      YANG_TYPE_EMPTY);
        int empty_count = 0;
        for (int i = 0; i < count; i++) {
            yang_data_node_t *leaf = yang_data_node_create(names[i],
                                                           YANG_KIND_LEAF,
                                                           YANG_TYPE_STRING);
            SET_STR(leaf->value, values[i]);
            if (values[i][0] == '\0') empty_count++;
            yang_data_add_child(cfg, leaf);
        }
        /* 若所有子节点均为空（即全部为自闭合标签），按 NETCONF 语义视为删除 */
        if (empty_count == count && count > 0) {
            op = NETCONF_EDIT_DELETE;
        }
        netconf_result_t rc = netconf_op_edit_config(s, ds_name, cfg, op);
        yang_data_node_free(cfg);
        if (rc == NETCONF_OK) {
            return write_ok_reply(msg_id, reply_out, reply_size);
        }
        return rc;
    }

    if (strcmp(op_name, "copy-config") == 0) {
        char src_xml[256], dst_xml[64];
        const char *sp = netconf_xml_find(rpc_in, "source", src_xml, sizeof(src_xml));
        const char *tp = netconf_xml_find(rpc_in, "target", dst_xml, sizeof(dst_xml));
        if (!sp || !tp) return NETCONF_ERR_INVALID_RPC;

        char src_name[64], dst_name[64];
        src_name[0] = dst_name[0] = '\0';

        const char *r = strchr(src_xml, '<');
        if (r) {
            r++;
            const char *re = r;
            while (*re && (isalnum((unsigned char)*re) || *re == '-')) re++;
            size_t n = (size_t)(re - r);
            if (n > 0 && n + 1 <= sizeof(src_name)) {
                memcpy(src_name, r, n); src_name[n] = '\0';
            }
        }
        r = strchr(dst_xml, '<');
        if (r) {
            r++;
            const char *re = r;
            while (*re && (isalnum((unsigned char)*re) || *re == '-')) re++;
            size_t n = (size_t)(re - r);
            if (n > 0 && n + 1 <= sizeof(dst_name)) {
                memcpy(dst_name, r, n); dst_name[n] = '\0';
            }
        }

        netconf_result_t rc = netconf_op_copy_config(s, src_name, dst_name);
        if (rc == NETCONF_OK) {
            return write_ok_reply(msg_id, reply_out, reply_size);
        }
        return rc;
    }

    if (strcmp(op_name, "delete-config") == 0) {
        char target_xml[64];
        if (!netconf_xml_find(rpc_in, "target", target_xml, sizeof(target_xml))) {
            return NETCONF_ERR_INVALID_RPC;
        }
        char ds_name[64] = {0};
        const char *r = strchr(target_xml, '<');
        if (r) {
            r++;
            const char *re = r;
            while (*re && (isalnum((unsigned char)*re) || *re == '-')) re++;
            size_t n = (size_t)(re - r);
            if (n > 0 && n + 1 <= sizeof(ds_name)) {
                memcpy(ds_name, r, n); ds_name[n] = '\0';
            }
        }
        if (!ds_name[0]) return NETCONF_ERR_INVALID_RPC;

        netconf_result_t rc = netconf_op_delete_config(s, ds_name);
        if (rc == NETCONF_OK) {
            return write_ok_reply(msg_id, reply_out, reply_size);
        }
        return rc;
    }

    /* NETCONF 1.1: get-capabilities */
    if (strcmp(op_name, "get-capabilities") == 0) {
        char caps_buf[4096];
        caps_buf[0] = '\0';
        int n = snprintf(caps_buf, sizeof(caps_buf),
            "<capabilities>"
            "<capability>urn:ietf:params:netconf:base:1.0</capability>"
            "<capability>urn:ietf:params:netconf:base:1.1</capability>"
            "<capability>urn:ietf:params:netconf:capability:notification:1.0</capability>"
            "</capabilities>");
        if (n < 0 || (size_t)n >= sizeof(caps_buf)) return NETCONF_ERR_INTERNAL;
        int reply_n = snprintf(reply_out, reply_size,
            "<rpc-reply xmlns=\"%s\" xmlns:nc=\"%s\" message-id=\"%s\">"
            "%s"
            "</rpc-reply>",
            NETCONF_NS, NETCONF_NS_1_1, msg_id ? msg_id : "0", caps_buf);
        if (reply_n < 0 || (size_t)reply_n >= reply_size) return NETCONF_ERR_INTERNAL;
        return NETCONF_OK;
    }

    return NETCONF_ERR_UNKNOWN_OP;
}