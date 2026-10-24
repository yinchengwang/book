/**
 * @file namespace.c
 * @brief 命名空间隔离与资源配额实现（多租户）
 *
 * 采用全局链表存储所有命名空间；quota 以字符串保存并提供简单解析。
 * usage 输出包含 vectors_used、quota 信息的 JSON。
 * double drop 返回 MMDB_ERR_INVALID。
 */
#include "sdk/mmdb_namespace.h"
#include "sdk/impl/mmdb_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ======================================================================== */
/* 内部结构                                                                 */
/* ======================================================================== */

/* 命名空间内部结构 */
struct mmdb_namespace_s {
    mmdb_t*                  db;                 /* 数据库句柄 */
    char*                    name;               /* 命名空间名称 */
    char*                    quota;              /* 配额原文 */
    uint64_t                 vectors_used;       /* 已用向量数 */
    int                      dropped;            /* 已释放标记 */
    struct mmdb_namespace_s* next;               /* 链表下一项 */
};

/* 全局命名空间链表头 */
static mmdb_namespace_t* g_namespaces = NULL;

/* ======================================================================== */
/* 内部辅助                                                                 */
/* ======================================================================== */

/* 分配并复制字符串 */
static char* ns_strdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* p = (char*)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

/* 查找命名空间（按名称） */
static mmdb_namespace_t* find_namespace(mmdb_t* db, const char* name) {
    (void)db;
    mmdb_namespace_t* ns = g_namespaces;
    while (ns) {
        if (!ns->dropped && strcmp(ns->name, name) == 0) return ns;
        ns = ns->next;
    }
    return NULL;
}

/* 简易 JSON 转义（仅处理双引号与反斜杠） */
static size_t json_escape(const char* src, char* dst, size_t dst_size) {
    size_t i = 0;
    while (*src) {
        if (*src == '"' || *src == '\\') {
            if (i + 2 >= dst_size) break;
            dst[i++] = '\\';
            dst[i++] = *src++;
        } else {
            if (i + 1 >= dst_size) break;
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
    return i;
}

/* ======================================================================== */
/* 公开 API                                                                 */
/* ======================================================================== */

int mmdb_namespace_create(mmdb_t* db, const char* name, const char* quota,
                          mmdb_namespace_t** out_ns) {
    if (!db || !name || !out_ns) return MMDB_ERR_INVALID;
    if (name[0] == '\0') return MMDB_ERR_INVALID;

    /* 检查是否已存在 */
    if (find_namespace(db, name)) {
        return MMDB_ERR_ALREADY;
    }

    /* 分配命名空间结构 */
    mmdb_namespace_t* ns = (mmdb_namespace_t*)calloc(1, sizeof(mmdb_namespace_t));
    if (!ns) return MMDB_ERR_NOMEM;

    ns->db = db;
    ns->name = ns_strdup(name);
    if (!ns->name) {
        free(ns);
        return MMDB_ERR_NOMEM;
    }

    if (quota) {
        ns->quota = ns_strdup(quota);
        if (!ns->quota) {
            free(ns->name);
            free(ns);
            return MMDB_ERR_NOMEM;
        }
    } else {
        ns->quota = ns_strdup("{}");
        if (!ns->quota) {
            free(ns->name);
            free(ns);
            return MMDB_ERR_NOMEM;
        }
    }

    /* 插入链表头部 */
    ns->next = g_namespaces;
    g_namespaces = ns;

    *out_ns = ns;
    return MMDB_OK;
}

int mmdb_namespace_get(mmdb_t* db, const char* name, mmdb_namespace_t** out_ns) {
    if (!db || !name || !out_ns) return MMDB_ERR_INVALID;

    mmdb_namespace_t* ns = find_namespace(db, name);
    if (!ns) return MMDB_ERR_NOT_FOUND;

    *out_ns = ns;
    return MMDB_OK;
}

int mmdb_namespace_set_quota(mmdb_namespace_t* ns, const char* quota) {
    if (!ns || !quota) return MMDB_ERR_INVALID;
    if (ns->dropped) return MMDB_ERR_INVALID;

    char* new_quota = ns_strdup(quota);
    if (!new_quota) return MMDB_ERR_NOMEM;

    free(ns->quota);
    ns->quota = new_quota;
    return MMDB_OK;
}

int mmdb_namespace_usage(mmdb_namespace_t* ns, char* json_out, size_t json_size) {
    if (!ns || !json_out || json_size == 0) return MMDB_ERR_INVALID;
    if (ns->dropped) return MMDB_ERR_INVALID;

    /* 简易拼接 JSON（避免引入 cjson） */
    size_t written = 0;
    written += (size_t)snprintf(json_out + written, json_size - written,
                                "{\"vectors_used\":%llu,\"quota\":%s}",
                                (unsigned long long)ns->vectors_used,
                                ns->quota ? ns->quota : "{}");
    if (written >= json_size) written = json_size - 1;
    json_out[written] = '\0';
    return MMDB_OK;
}

int mmdb_namespace_drop(mmdb_namespace_t* ns) {
    if (!ns) return MMDB_ERR_INVALID;
    if (ns->dropped) return MMDB_ERR_INVALID;

    /* 标记为已释放（链表中保留节点，避免悬垂指针） */
    ns->dropped = 1;
    return MMDB_OK;
}
