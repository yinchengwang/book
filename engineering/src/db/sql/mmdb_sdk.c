/**
 * @file mmdb_sdk.c
 * @brief 多模态数据库 SDK 实现
 */

#include "db/sql/mmdb_sdk.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* ========================================================================
 * 连接管理
 * ======================================================================== */

/** 内部连接数据 */
typedef struct MMDBConnectionInternal_s {
    char recv_buf[8192];
    int recv_pos;
    int recv_len;
    MMDBErrorCode error_code;
    char error_msg[512];
} MMDBConnectionInternal;

static int socket_connect(const char *host, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        /* 尝试解析主机名 */
        struct hostent *he = gethostbyname(host);
        if (!he) {
            close(fd);
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    /* 设置 TCP 选项 */
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    return fd;
}

MMDBConnection *mmdb_connect(const char *host, int port,
                           const char *user, const char *password,
                           const char *database)
{
    MMDBConnection *conn = (MMDBConnection *)calloc(1, sizeof(MMDBConnection));
    if (!conn) return NULL;

    conn->host = host ? strdup(host) : strdup("localhost");
    conn->port = port > 0 ? port : 5432;
    conn->user = user ? strdup(user) : strdup("postgres");
    conn->password = password ? strdup(password) : strdup("");
    conn->database = database ? strdup(database) : strdup("postgres");
    conn->connected = false;

    conn->internal = calloc(1, sizeof(MMDBConnectionInternal));
    if (!conn->internal) {
        free(conn->host);
        free(conn->user);
        free(conn->password);
        free(conn->database);
        free(conn);
        return NULL;
    }

    /* 建立连接 */
    conn->fd = socket_connect(conn->host, conn->port);
    if (conn->fd < 0) {
        MMDBConnectionInternal *intl = (MMDBConnectionInternal *)conn->internal;
        snprintf(intl->error_msg, sizeof(intl->error_msg),
                 "Failed to connect to %s:%d", conn->host, conn->port);
        intl->error_code = MMDB_ERROR_CONNECTION;
        free(conn->host);
        free(conn->user);
        free(conn->password);
        free(conn->database);
        free(conn->internal);
        free(conn);
        return NULL;
    }

    conn->connected = true;
    return conn;
}

void mmdb_disconnect(MMDBConnection *conn)
{
    if (!conn) return;

    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }

    free(conn->host);
    free(conn->user);
    free(conn->password);
    free(conn->database);

    if (conn->internal) {
        free(conn->internal);
    }

    free(conn);
}

bool mmdb_is_connected(const MMDBConnection *conn)
{
    return conn && conn->connected;
}

const char *mmdb_get_error(const MMDBConnection *conn)
{
    if (!conn || !conn->internal) {
        return "Unknown error";
    }
    MMDBConnectionInternal *intl = (MMDBConnectionInternal *)conn->internal;
    return intl->error_msg;
}

int mmdb_set_option(MMDBConnection *conn, const char *name, const char *value)
{
    (void)conn;
    (void)name;
    (void)value;
    return 0;
}

const char *mmdb_get_option(const MMDBConnection *conn, const char *name)
{
    (void)conn;
    (void)name;
    return NULL;
}

/* ========================================================================
 * SQL 执行
 * ======================================================================== */

/** 内部游标数据 */
typedef struct MMDBCursorInternal_s {
    char **rows;
    int nrows;
    int ncols;
    char **col_names;
    int *col_types;
} MMDBCursorInternal;

static int socket_send(int fd, const char *data, int len)
{
    int total = 0;
    while (total < len) {
        int n = send(fd, data + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += n;
    }
    return total;
}

static int socket_recv(int fd, char *buf, int len)
{
    int total = 0;
    while (total < len) {
        int n = recv(fd, buf + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return total;
        }
        total += n;
    }
    return total;
}

MMDBCursor *mmdb_execute(MMDBConnection *conn, const char *sql)
{
    if (!conn || !conn->connected || !sql) {
        return NULL;
    }

    /* 简化实现：通过 socket 发送查询 */
    /* 实际应使用 PostgreSQL Wire Protocol */
    int sql_len = strlen(sql);

    /* 发送 SQL 查询（简化格式：长度 + SQL） */
    char header[16];
    snprintf(header, sizeof(header), "%08x", sql_len);

    if (socket_send(conn->fd, sql, sql_len) < 0) {
        return NULL;
    }

    /* 创建游标 */
    MMDBCursor *cursor = (MMDBCursor *)calloc(1, sizeof(MMDBCursor));
    if (!cursor) return NULL;

    cursor->conn = conn;
    cursor->row_count = 0;
    cursor->col_count = 0;
    cursor->current_row = -1;

    cursor->internal = calloc(1, sizeof(MMDBCursorInternal));
    MMDBCursorInternal *intl = (MMDBCursorInternal *)cursor->internal;
    intl->rows = NULL;
    intl->nrows = 0;
    intl->ncols = 0;
    intl->col_names = NULL;
    intl->col_types = NULL;

    /* TODO: 实际接收查询结果 */
    /* 目前返回空结果集 */
    return cursor;
}

int64_t mmdb_execute_update(MMDBConnection *conn, const char *sql)
{
    MMDBCursor *cursor = mmdb_execute(conn, sql);
    if (!cursor) {
        return -1;
    }

    int64_t affected = cursor->row_count;
    mmdb_cursor_close(cursor);

    return affected;
}

int mmdb_prepare(MMDBConnection *conn, const char *sql)
{
    (void)conn;
    (void)sql;
    /* TODO: 实现预处理语句 */
    return 0;
}

MMDBCursor *mmdb_execute_prepared(MMDBConnection *conn, int stmt_id,
                                  void **params, int nparams)
{
    (void)conn;
    (void)stmt_id;
    (void)params;
    (void)nparams;
    /* TODO: 执行预处理语句 */
    return NULL;
}

void mmdb_cursor_close(MMDBCursor *cursor)
{
    if (!cursor) return;

    if (cursor->internal) {
        MMDBCursorInternal *intl = (MMDBCursorInternal *)cursor->internal;

        /* 释放行数据 */
        for (int i = 0; i < intl->nrows; i++) {
            free(intl->rows[i]);
        }
        free(intl->rows);

        /* 释放列信息 */
        for (int i = 0; i < intl->ncols; i++) {
            free(intl->col_names[i]);
        }
        free(intl->col_names);
        free(intl->col_types);

        free(cursor->internal);
    }

    free(cursor);
}

int mmdb_get_column_count(const MMDBCursor *cursor)
{
    if (!cursor || !cursor->internal) return 0;
    return ((MMDBCursorInternal *)cursor->internal)->ncols;
}

int64_t mmdb_get_row_count(const MMDBCursor *cursor)
{
    return cursor ? cursor->row_count : 0;
}

const char *mmdb_get_column_name(const MMDBCursor *cursor, int col)
{
    if (!cursor || !cursor->internal) return NULL;
    MMDBCursorInternal *intl = (MMDBCursorInternal *)cursor->internal;
    if (col < 0 || col >= intl->ncols) return NULL;
    return intl->col_names[col];
}

int mmdb_get_column_type(const MMDBCursor *cursor, int col)
{
    if (!cursor || !cursor->internal) return MMDB_NULL;
    MMDBCursorInternal *intl = (MMDBCursorInternal *)cursor->internal;
    if (col < 0 || col >= intl->ncols) return MMDB_NULL;
    return intl->col_types[col];
}

const char *mmdb_get_column_type_name(const MMDBCursor *cursor, int col)
{
    return mmdb_type_name((MMDBType)mmdb_get_column_type(cursor, col));
}

bool mmdb_cursor_next(MMDBCursor *cursor)
{
    if (!cursor || !cursor->internal) return false;
    MMDBCursorInternal *intl = (MMDBCursorInternal *)cursor->internal;

    cursor->current_row++;
    return cursor->current_row < intl->nrows;
}

void mmdb_cursor_reset(MMDBCursor *cursor)
{
    if (cursor) {
        cursor->current_row = -1;
    }
}

int64_t mmdb_get_int(MMDBCursor *cursor, int col)
{
    if (!cursor || !cursor->internal) return 0;
    MMDBCursorInternal *intl = (MMDBCursorInternal *)cursor->internal;

    if (cursor->current_row < 0 || cursor->current_row >= intl->nrows) {
        return 0;
    }

    char *row = intl->rows[cursor->current_row];
    char *val = row;

    /* 跳过前面的列 */
    for (int i = 0; i < col && *val; i++) {
        val = strchr(val, '\t');
        if (!val) return 0;
        val++;
    }

    if (!val || !*val) return 0;
    if (strcmp(val, "NULL") == 0) return 0;

    return atoll(val);
}

int64_t mmdb_get_int64(MMDBCursor *cursor, int col)
{
    return mmdb_get_int(cursor, col);
}

double mmdb_get_double(MMDBCursor *cursor, int col)
{
    if (!cursor || !cursor->internal) return 0.0;
    MMDBCursorInternal *intl = (MMDBCursorInternal *)cursor->internal;

    if (cursor->current_row < 0 || cursor->current_row >= intl->nrows) {
        return 0.0;
    }

    char *row = intl->rows[cursor->current_row];
    char *val = row;

    for (int i = 0; i < col && *val; i++) {
        val = strchr(val, '\t');
        if (!val) return 0.0;
        val++;
    }

    if (!val || !*val) return 0.0;
    if (strcmp(val, "NULL") == 0) return 0.0;

    return atof(val);
}

const char *mmdb_get_string(MMDBCursor *cursor, int col)
{
    if (!cursor || !cursor->internal) return NULL;
    MMDBCursorInternal *intl = (MMDBCursorInternal *)cursor->internal;

    if (cursor->current_row < 0 || cursor->current_row >= intl->nrows) {
        return NULL;
    }

    char *row = intl->rows[cursor->current_row];
    static char buf[4096];
    char *val = row;

    for (int i = 0; i < col && *val; i++) {
        val = strchr(val, '\t');
        if (!val) return NULL;
        val++;
    }

    if (!val || !*val) return NULL;

    /* 复制到静态缓冲区 */
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* 截断到行尾 */
    char *end = strchr(buf, '\n');
    if (end) *end = '\0';
    end = strchr(buf, '\r');
    if (end) *end = '\0';

    return buf;
}

const void *mmdb_get_bytes(MMDBCursor *cursor, int col, int *len)
{
    (void)cursor;
    (void)col;
    if (len) *len = 0;
    return NULL;
}

bool mmdb_get_bool(MMDBCursor *cursor, int col)
{
    const char *val = mmdb_get_string(cursor, col);
    if (!val) return false;
    return strcmp(val, "t") == 0 || strcmp(val, "true") == 0 ||
           strcmp(val, "1") == 0 || strcmp(val, "yes") == 0;
}

bool mmdb_is_null(MMDBCursor *cursor, int col)
{
    const char *val = mmdb_get_string(cursor, col);
    return val == NULL || strcmp(val, "NULL") == 0;
}

/* ========================================================================
 * 向量操作
 * ======================================================================== */

MMDBVector *mmdb_vector_create(float *data, int dim)
{
    if (!data || dim <= 0) return NULL;

    MMDBVector *vec = (MMDBVector *)calloc(1, sizeof(MMDBVector));
    if (!vec) return NULL;

    vec->dim = dim;
    vec->size = dim * sizeof(float);
    vec->data = (float *)malloc(vec->size);
    if (!vec->data) {
        free(vec);
        return NULL;
    }

    memcpy(vec->data, data, vec->size);
    return vec;
}

void mmdb_vector_destroy(MMDBVector *vec)
{
    if (!vec) return;
    free(vec->data);
    free(vec);
}

MMDBVector *mmdb_vector_copy(const MMDBVector *vec)
{
    if (!vec) return NULL;
    return mmdb_vector_create(vec->data, vec->dim);
}

int64_t mmdb_vector_insert(MMDBConnection *conn, const char *collection,
                          const MMDBVector *vec, const char *metadata)
{
    if (!conn || !conn->connected || !collection || !vec) {
        return -1;
    }

    /* 构建 SQL */
    char sql[8192];
    char *vec_str = (char *)malloc(vec->dim * 16);
    int pos = 0;

    for (int i = 0; i < vec->dim; i++) {
        if (i > 0) vec_str[pos++] = ',';
        pos += snprintf(vec_str + pos, 64, "%g", vec->data[i]);
    }

    if (metadata) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO \"%s\" (embedding, metadata) VALUES ([%s], '%s')",
                 collection, vec_str, metadata);
    } else {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO \"%s\" (embedding) VALUES ([%s])",
                 collection, vec_str);
    }

    free(vec_str);
    return mmdb_execute_update(conn, sql);
}

int64_t mmdb_vector_insert_batch(MMDBConnection *conn, const char *collection,
                                MMDBVector **vecs, int nvecs)
{
    if (!conn || !collection || !vecs || nvecs <= 0) {
        return -1;
    }

    int64_t total = 0;
    for (int i = 0; i < nvecs; i++) {
        if (mmdb_vector_insert(conn, collection, vecs[i], NULL) > 0) {
            total++;
        }
    }

    return total;
}

MMDBCursor *mmdb_vector_search(MMDBConnection *conn, const char *collection,
                               const MMDBVector *query, int top_k)
{
    if (!conn || !conn->connected || !collection || !query) {
        return NULL;
    }

    /* 构建 SQL */
    char sql[8192];
    char *vec_str = (char *)malloc(query->dim * 16);
    int pos = 0;

    for (int i = 0; i < query->dim; i++) {
        if (i > 0) vec_str[pos++] = ',';
        pos += snprintf(vec_str + pos, 64, "%g", query->data[i]);
    }

    snprintf(sql, sizeof(sql),
             "SELECT * FROM \"%s\" ORDER BY embedding <=> [%s] LIMIT %d",
             collection, vec_str, top_k);

    free(vec_str);
    return mmdb_execute(conn, sql);
}

MMDBCursor *mmdb_vector_search_filter(MMDBConnection *conn,
                                      const char *collection,
                                      const MMDBVector *query,
                                      int top_k,
                                      const char *filter)
{
    if (!conn || !conn->connected || !collection || !query) {
        return NULL;
    }

    /* 构建 SQL */
    char sql[8192];
    char *vec_str = (char *)malloc(query->dim * 16);
    int pos = 0;

    for (int i = 0; i < query->dim; i++) {
        if (i > 0) vec_str[pos++] = ',';
        pos += snprintf(vec_str + pos, 64, "%g", query->data[i]);
    }

    if (filter) {
        snprintf(sql, sizeof(sql),
                 "SELECT * FROM \"%s\" WHERE %s ORDER BY embedding <=> [%s] LIMIT %d",
                 collection, filter, vec_str, top_k);
    } else {
        snprintf(sql, sizeof(sql),
                 "SELECT * FROM \"%s\" ORDER BY embedding <=> [%s] LIMIT %d",
                 collection, vec_str, top_k);
    }

    free(vec_str);
    return mmdb_execute(conn, sql);
}

int64_t mmdb_vector_delete(MMDBConnection *conn, const char *collection,
                          int64_t *ids, int nids)
{
    if (!conn || !conn->connected || !collection || !ids || nids <= 0) {
        return -1;
    }

    /* 构建 IN 子句 */
    char id_list[4096];
    int pos = 0;
    for (int i = 0; i < nids; i++) {
        if (i > 0) id_list[pos++] = ',';
        pos += snprintf(id_list + pos, 64, "%lld", (long long)ids[i]);
    }

    char sql[8192];
    snprintf(sql, sizeof(sql),
             "DELETE FROM \"%s\" WHERE id IN (%s)",
             collection, id_list);

    return mmdb_execute_update(conn, sql);
}

MMDBCursor *mmdb_vector_get(MMDBConnection *conn, const char *collection,
                           int64_t id)
{
    if (!conn || !conn->connected || !collection) {
        return NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT * FROM \"%s\" WHERE id = %lld",
             collection, (long long)id);

    return mmdb_execute(conn, sql);
}

int mmdb_vector_update(MMDBConnection *conn, const char *collection,
                      int64_t id, const MMDBVector *vec)
{
    if (!conn || !conn->connected || !collection || !vec) {
        return -1;
    }

    /* 构建 SQL */
    char sql[8192];
    char *vec_str = (char *)malloc(vec->dim * 16);
    int pos = 0;

    for (int i = 0; i < vec->dim; i++) {
        if (i > 0) vec_str[pos++] = ',';
        pos += snprintf(vec_str + pos, 64, "%g", vec->data[i]);
    }

    snprintf(sql, sizeof(sql),
             "UPDATE \"%s\" SET embedding = [%s] WHERE id = %lld",
             collection, vec_str, (long long)id);

    free(vec_str);

    return mmdb_execute_update(conn, sql) >= 0 ? 0 : -1;
}

/* ========================================================================
 * 集合管理
 * ======================================================================== */

MMDBCollectionInfo *mmdb_collection_info(MMDBConnection *conn,
                                       const char *name)
{
    if (!conn || !conn->connected || !name) {
        return NULL;
    }

    MMDBCollectionInfo *info = (MMDBCollectionInfo *)calloc(1,
        sizeof(MMDBCollectionInfo));
    if (!info) return NULL;

    strncpy(info->name, name, sizeof(info->name) - 1);
    info->vector_dim = 0;
    info->count = 0;
    info->size_bytes = 0;

    /* TODO: 从系统表获取实际信息 */
    return info;
}

MMDBCursor *mmdb_collection_list(MMDBConnection *conn)
{
    if (!conn || !conn->connected) {
        return NULL;
    }

    /* 查询系统表获取集合列表 */
    return mmdb_execute(conn,
        "SELECT name, vector_dim, count FROM information_schema.collections");
}

void mmdb_collection_info_destroy(MMDBCollectionInfo *info)
{
    free(info);
}

/* ========================================================================
 * 集合管理（DDL）
 * ======================================================================== */

int mmdb_collection_create(MMDBConnection *conn, const char *name,
                          int dim, const char *config)
{
    if (!conn || !conn->connected || !name || dim <= 0) {
        return -1;
    }

    char sql[4096];

    if (config) {
        snprintf(sql, sizeof(sql),
                 "CREATE TABLE \"%s\" (id BIGSERIAL PRIMARY KEY, embedding vector(%d), metadata jsonb)",
                 name, dim);
    } else {
        snprintf(sql, sizeof(sql),
                 "CREATE TABLE \"%s\" (id BIGSERIAL PRIMARY KEY, embedding vector(%d))",
                 name, dim);
    }

    return mmdb_execute_update(conn, sql) >= 0 ? 0 : -1;
}

int mmdb_collection_drop(MMDBConnection *conn, const char *name)
{
    if (!conn || !conn->connected || !name) {
        return -1;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "DROP TABLE \"%s\"", name);

    return mmdb_execute_update(conn, sql) >= 0 ? 0 : -1;
}

/* ========================================================================
 * 事务管理
 * ======================================================================== */

int mmdb_begin(MMDBConnection *conn)
{
    return mmdb_execute_update(conn, "BEGIN") >= 0 ? 0 : -1;
}

int mmdb_commit(MMDBConnection *conn)
{
    return mmdb_execute_update(conn, "COMMIT") >= 0 ? 0 : -1;
}

int mmdb_rollback(MMDBConnection *conn)
{
    return mmdb_execute_update(conn, "ROLLBACK") >= 0 ? 0 : -1;
}

int mmdb_savepoint(MMDBConnection *conn, const char *name)
{
    if (!conn || !name) return -1;

    char sql[256];
    snprintf(sql, sizeof(sql), "SAVEPOINT %s", name);
    return mmdb_execute_update(conn, sql) >= 0 ? 0 : -1;
}

int mmdb_rollback_to(MMDBConnection *conn, const char *name)
{
    if (!conn || !name) return -1;

    char sql[256];
    snprintf(sql, sizeof(sql), "ROLLBACK TO SAVEPOINT %s", name);
    return mmdb_execute_update(conn, sql) >= 0 ? 0 : -1;
}

int mmdb_release_savepoint(MMDBConnection *conn, const char *name)
{
    if (!conn || !name) return -1;

    char sql[256];
    snprintf(sql, sizeof(sql), "RELEASE SAVEPOINT %s", name);
    return mmdb_execute_update(conn, sql) >= 0 ? 0 : -1;
}

/* ========================================================================
 * 连接池
 * ======================================================================== */

/** 连接池节点 */
typedef struct MMPoolNode_s {
    MMDBConnection *conn;
    bool in_use;
    struct MMPoolNode_s *next;
} MMPoolNode;

struct MMDBPool_s {
    char *host;
    int port;
    char *user;
    char *password;
    char *database;
    int max_size;
    int min_size;

    MMPoolNode *free_list;
    MMPoolNode *used_list;
    int total_count;
};

MMDBPool *mmdb_pool_create(const char *host, int port,
                          const char *user, const char *password,
                          const char *database, int max_size)
{
    MMDBPool *pool = (MMDBPool *)calloc(1, sizeof(MMDBPool));
    if (!pool) return NULL;

    pool->host = host ? strdup(host) : strdup("localhost");
    pool->port = port > 0 ? port : 5432;
    pool->user = user ? strdup(user) : strdup("postgres");
    pool->password = password ? strdup(password) : strdup("");
    pool->database = database ? strdup(database) : strdup("postgres");
    pool->max_size = max_size > 0 ? max_size : 10;
    pool->min_size = 0;
    pool->free_list = NULL;
    pool->used_list = NULL;
    pool->total_count = 0;

    return pool;
}

void mmdb_pool_destroy(MMDBPool *pool)
{
    if (!pool) return;

    /* 释放所有连接 */
    MMPoolNode *node = pool->free_list;
    while (node) {
        MMPoolNode *next = node->next;
        mmdb_disconnect(node->conn);
        free(node);
        node = next;
    }

    node = pool->used_list;
    while (node) {
        MMPoolNode *next = node->next;
        mmdb_disconnect(node->conn);
        free(node);
        node = next;
    }

    free(pool->host);
    free(pool->user);
    free(pool->password);
    free(pool->database);
    free(pool);
}

MMDBConnection *mmdb_pool_get_connection(MMDBPool *pool)
{
    if (!pool) return NULL;

    /* 从空闲列表获取 */
    if (pool->free_list) {
        MMPoolNode *node = pool->free_list;
        pool->free_list = node->next;

        /* 添加到使用列表 */
        node->next = pool->used_list;
        node->in_use = true;
        pool->used_list = node;

        return node->conn;
    }

    /* 检查是否达到最大连接数 */
    if (pool->total_count >= pool->max_size) {
        /* 等待可用连接 */
        return NULL;
    }

    /* 创建新连接 */
    MMDBConnection *conn = mmdb_connect(pool->host, pool->port,
                                         pool->user, pool->password,
                                         pool->database);
    if (!conn) {
        return NULL;
    }

    MMPoolNode *node = (MMPoolNode *)malloc(sizeof(MMPoolNode));
    node->conn = conn;
    node->in_use = true;
    node->next = pool->used_list;
    pool->used_list = node;
    pool->total_count++;

    return conn;
}

void mmdb_pool_release(MMDBPool *pool, MMDBConnection *conn)
{
    if (!pool || !conn) return;

    /* 从使用列表移动到空闲列表 */
    MMPoolNode **prev = &pool->used_list;
    MMPoolNode *node = pool->used_list;

    while (node) {
        if (node->conn == conn) {
            *prev = node->next;
            node->in_use = false;
            node->next = pool->free_list;
            pool->free_list = node;
            return;
        }
        prev = &node->next;
        node = node->next;
    }
}

void mmdb_pool_refresh(MMDBPool *pool)
{
    if (!pool) return;

    /* 关闭所有连接并重新创建 */
    MMPoolNode *node = pool->free_list;
    while (node) {
        MMPoolNode *next = node->next;
        mmdb_disconnect(node->conn);
        free(node);
        node = next;
    }
    pool->free_list = NULL;

    node = pool->used_list;
    while (node) {
        MMPoolNode *next = node->next;
        mmdb_disconnect(node->conn);
        free(node);
        node = next;
    }
    pool->used_list = NULL;
    pool->total_count = 0;
}

/* ========================================================================
 * 类型映射
 * ======================================================================== */

const char *mmdb_type_name(MMDBType type)
{
    switch (type) {
        case MMDB_NULL: return "NULL";
        case MMDB_BOOL: return "BOOL";
        case MMDB_INT: return "INT";
        case MMDB_INT64: return "INT64";
        case MMDB_FLOAT: return "FLOAT";
        case MMDB_DOUBLE: return "DOUBLE";
        case MMDB_STRING: return "STRING";
        case MMDB_BYTES: return "BYTES";
        case MMDB_VECTOR: return "VECTOR";
        case MMDB_TIMESTAMP: return "TIMESTAMP";
        case MMDB_DATE: return "DATE";
        case MMDB_JSON: return "JSON";
        case MMDB_ARRAY: return "ARRAY";
        case MMDB_OBJECT: return "OBJECT";
        default: return "UNKNOWN";
    }
}

MMDBType mmdb_type_from_name(const char *name)
{
    if (!name) return MMDB_NULL;

    if (strcmp(name, "NULL") == 0) return MMDB_NULL;
    if (strcmp(name, "BOOL") == 0) return MMDB_BOOL;
    if (strcmp(name, "INT") == 0) return MMDB_INT;
    if (strcmp(name, "INT64") == 0) return MMDB_INT64;
    if (strcmp(name, "FLOAT") == 0) return MMDB_FLOAT;
    if (strcmp(name, "DOUBLE") == 0) return MMDB_DOUBLE;
    if (strcmp(name, "STRING") == 0) return MMDB_STRING;
    if (strcmp(name, "BYTES") == 0) return MMDB_BYTES;
    if (strcmp(name, "VECTOR") == 0) return MMDB_VECTOR;
    if (strcmp(name, "TIMESTAMP") == 0) return MMDB_TIMESTAMP;
    if (strcmp(name, "DATE") == 0) return MMDB_DATE;
    if (strcmp(name, "JSON") == 0) return MMDB_JSON;
    if (strcmp(name, "ARRAY") == 0) return MMDB_ARRAY;
    if (strcmp(name, "OBJECT") == 0) return MMDB_OBJECT;

    return MMDB_NULL;
}

void mmdb_free(void *ptr)
{
    free(ptr);
}

/* ========================================================================
 * 错误处理
 * ======================================================================== */

MMDBErrorCode mmdb_error_code(const MMDBConnection *conn)
{
    if (!conn || !conn->internal) {
        return MMDB_ERROR_UNKNOWN;
    }
    return ((MMDBConnectionInternal *)conn->internal)->error_code;
}

const char *mmdb_error_message(const MMDBConnection *conn)
{
    if (!conn || !conn->internal) {
        return "Unknown error";
    }
    return ((MMDBConnectionInternal *)conn->internal)->error_msg;
}

const char *mmdb_sqlstate(const MMDBConnection *conn)
{
    (void)conn;
    /* TODO: 返回 SQL 状态码 */
    return "00000";
}
