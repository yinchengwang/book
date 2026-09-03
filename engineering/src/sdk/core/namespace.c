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
    uint64_t                 collections_used;   /* 已用 Collection 数量 */
    size_t                   disk_used_bytes;    /* 已用磁盘量（字节） */
    uint64_t                 vectors_limit;      /* 向量配额上限 */
    uint64_t                 collections_limit;  /* Collection 配额上限 */
    size_t                   disk_limit_bytes;   /* 磁盘配额上限 */
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

/* 简易 JSON 配额解析（仅提取三个数值字段） */
static int parse_quota(const char* json, uint64_t* vectors_max,
                       uint64_t* collections_max, size_t* disk_max_bytes) {
    *vectors_max = 0;
    *collections_max = 0;
    *disk_max_bytes = 0;

    if (!json || json[0] == '\0') return MMDB_OK;

    /* 简单字符串扫描，寻找 "vectors_max":N 等模式 */
    const char* p;

    p = strstr(json, "\"vectors_max\"");
    if (p) {
        p += strlen("\"vectors_max\"");
        while (*p == ' ' || *p == ':' || *p == ',') p++;
        *vectors_max = (uint64_t)strtoull(p, NULL, 10);
    }

    p = strstr(json, "\"collections_max\"");
    if (p) {
        p += strlen("\"collections_max\"");
        while (*p == ' ' || *p == ':' || *p == ',') p++;
        *collections_max = (uint64_t)strtoull(p, NULL, 10);
    }

    p = strstr(json, "\"disk_max_bytes\"");
    if (p) {
        p += strlen("\"disk_max_bytes\"");
        while (*p == ' ' || *p == ':' || *p == ',') p++;
        *disk_max_bytes = (size_t)strtoull(p, NULL, 10);
    }

    return MMDB_OK;
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

    /* 解析配额 */
    parse_quota(ns->quota, &ns->vectors_limit, &ns->collections_limit,
                &ns->disk_limit_bytes);

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

    /* 重新解析配额 */
    parse_quota(ns->quota, &ns->vectors_limit, &ns->collections_limit,
                &ns->disk_limit_bytes);

    return MMDB_OK;
}

int mmdb_namespace_usage(mmdb_namespace_t* ns, char* json_out, size_t json_size) {
    if (!ns || !json_out || json_size == 0) return MMDB_ERR_INVALID;
    if (ns->dropped) return MMDB_ERR_INVALID;

    /* 简易拼接 JSON */
    size_t written = (size_t)snprintf(json_out, json_size,
        "{\"vectors_used\":%llu,\"collections_used\":%llu,"
        "\"disk_used_bytes\":%llu,"
        "\"vectors_limit\":%llu,\"collections_limit\":%llu,"
        "\"disk_limit_bytes\":%llu}",
        (unsigned long long)ns->vectors_used,
        (unsigned long long)ns->collections_used,
        (unsigned long long)ns->disk_used_bytes,
        (unsigned long long)ns->vectors_limit,
        (unsigned long long)ns->collections_limit,
        (unsigned long long)ns->disk_limit_bytes);

    if (written >= json_size) {
        json_out[json_size - 1] = '\0';
    }
    return MMDB_OK;
}

int mmdb_namespace_check_quota(mmdb_namespace_t* ns, uint64_t extra_vectors) {
    if (!ns) return MMDB_ERR_INVALID;
    if (ns->dropped) return MMDB_ERR_INVALID;

    /* 0 表示无限制 */
    if (ns->vectors_limit == 0) return MMDB_OK;

    if (ns->vectors_used + extra_vectors > ns->vectors_limit) {
        return MMDB_ERR_FULL;
    }
    return MMDB_OK;
}

int mmdb_namespace_add_vectors(mmdb_namespace_t* ns, uint64_t add_count) {
    if (!ns) return MMDB_ERR_INVALID;
    if (ns->dropped) return MMDB_ERR_INVALID;

    ns->vectors_used += add_count;
    return MMDB_OK;
}

int mmdb_namespace_drop(mmdb_namespace_t* ns) {
    if (!ns) return MMDB_ERR_INVALID;
    if (ns->dropped) return MMDB_ERR_INVALID;

    /* 标记为已释放（链表中保留节点，避免悬垂指针） */
    ns->dropped = 1;

    /* 释放内部字符串 */
    free(ns->name);
    ns->name = NULL;
    free(ns->quota);
    ns->quota = NULL;

    return MMDB_OK;
}

const char* mmdb_namespace_name(const mmdb_namespace_t* ns) {
    if (!ns) return NULL;
    return ns->name;
}