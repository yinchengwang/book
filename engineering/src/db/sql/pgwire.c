/**
 * @file pgwire.c
 * @brief PostgreSQL Wire Protocol 实现
 *
 * Task 3.6: Windows 兼容层
 *   - Windows 使用 Winsock2（<winsock2.h> + <ws2tcpip.h>）
 *   - POSIX 使用 Berkeley socket（<sys/socket.h> + <arpa/inet.h> + <unistd.h>）
 *   - 统一抽象 close() 接口；fcntl 通过宏映射
 */

#include "db/sql/pgwire.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
/* Windows 平台：使用 Winsock2 */
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/* POSIX 兼容垫片 */
typedef int socklen_t;
#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifndef EAGAIN
#define EAGAIN WSAEWOULDBLOCK
#endif

#define close(fd) closesocket((SOCKET)(fd))
#define read(fd, buf, len) recv((SOCKET)(fd), (buf), (len), 0)
#define write(fd, buf, len) send((SOCKET)(fd), (buf), (len), 0)

/* strndup 在 Windows 上缺失 */
static inline char *strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }
    return copy;
}

/* htobe64 / be64toh 在 Windows 上缺失，使用内联实现 */
static inline uint64_t htobe64(uint64_t x) {
    union { uint32_t w[2]; uint64_t d; } u;
    u.d = x;
    uint32_t t = htonl(u.w[1]);
    u.w[1] = htonl(u.w[0]);
    u.w[0] = t;
    return u.d;
}
static inline uint64_t be64toh(uint64_t x) {
    union { uint32_t w[2]; uint64_t d; } u;
    u.d = x;
    uint32_t t = ntohl(u.w[1]);
    u.w[1] = ntohl(u.w[0]);
    u.w[0] = t;
    return u.d;
}

#else
/* POSIX 平台：使用 Berkeley socket */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

/* ========================================================================
 * 类型信息表
 * ======================================================================== */

static const PgTypeInfo g_type_info[] = {
    {PGTYPE_BOOL, "bool", 1, 1, true},
    {PGTYPE_INT2, "int2", 2, 2, true},
    {PGTYPE_INT4, "int4", 4, 4, true},
    {PGTYPE_INT8, "int8", 8, 8, true},
    {PGTYPE_FLOAT4, "float4", 4, 4, true},
    {PGTYPE_FLOAT8, "float8", 8, 8, true},
    {PGTYPE_TEXT, "text", -1, 4, false},
    {PGTYPE_VARCHAR, "varchar", -1, 4, false},
    {PGTYPE_CHAR, "bpchar", -1, 4, false},
    {PGTYPE_BYTEA, "bytea", -1, 4, false},
    {PGTYPE_NUMERIC, "numeric", -1, 4, false},
    {PGTYPE_DATE, "date", 4, 4, true},
    {PGTYPE_TIME, "time", 8, 8, true},
    {PGTYPE_TIMESTAMP, "timestamp", 8, 8, true},
    {PGTYPE_TIMESTAMPTZ, "timestamptz", 8, 8, true},
    {PGTYPE_INTERVAL, "interval", 16, 8, true},
    {PGTYPE_BOOLARRAY, "_bool", -1, 4, false},
    {PGTYPE_INT2ARRAY, "_int2", -1, 4, false},
    {PGTYPE_INT4ARRAY, "_int4", -1, 4, false},
    {PGTYPE_TEXTARRAY, "_text", -1, 4, false},
    {PGTYPE_JSON, "json", -1, 4, false},
    {PGTYPE_JSONB, "jsonb", -1, 4, false},
    {PGTYPE_XML, "xml", -1, 4, false},
    {PGTYPE_INET, "inet", -1, 4, false},
    {PGTYPE_CIDR, "cidr", -1, 4, false},
    {PGTYPE_MACADDR, "macaddr", 6, 1, true},
    {PGTYPE_POINT, "point", 16, 8, true},
    {PGTYPE_LSEG, "lseg", 32, 8, true},
    {PGTYPE_BOX, "box", 32, 8, true},
    {PGTYPE_POLYGON, "polygon", -1, 4, false},
    {PGTYPE_CIRCLE, "circle", 24, 8, true},
    {PGTYPE_VECTOR_F32, "vector", -1, 4, false},
    {PGTYPE_VECTOR_F16, "vector_f16", -1, 4, false},
    {PGTYPE_TIMESERIES, "timeseries", -1, 4, false},
    {PGTYPE_DOCUMENT, "document", -1, 4, false},
    {0, NULL, 0, 0, false}
};

/* ========================================================================
 * 缓冲区操作
 * ======================================================================== */

/**
 * @brief 确保缓冲区有足够空间
 */
static int ensure_write_space(PgwireConn *conn, int needed)
{
    if (conn->write_pos + needed <= conn->write_capacity) {
        return 0;
    }

    int new_capacity = conn->write_capacity * 2;
    if (new_capacity < conn->write_pos + needed + 8192) {
        new_capacity = conn->write_pos + needed + 8192;
    }

    char *new_buf = realloc(conn->write_buf, new_capacity);
    if (!new_buf) {
        return -1;
    }

    conn->write_buf = new_buf;
    conn->write_capacity = new_capacity;
    return 0;
}

/**
 * @brief 追加数据到写入缓冲区
 */
static int write_to_buffer(PgwireConn *conn, const void *data, int len)
{
    if (ensure_write_space(conn, len) < 0) {
        return -1;
    }
    memcpy(conn->write_buf + conn->write_pos, data, len);
    conn->write_pos += len;
    return 0;
}

/**
 * @brief 写入字节
 */
static int write_byte(PgwireConn *conn, char byte)
{
    return write_to_buffer(conn, &byte, 1);
}

/**
 * @brief 写入 int16
 */
static int write_int16(PgwireConn *conn, int16_t val)
{
    int16_t network = htons(val);
    return write_to_buffer(conn, &network, 2);
}

/**
 * @brief 写入 int32
 */
static int write_int32(PgwireConn *conn, int32_t val)
{
    int32_t network = htonl(val);
    return write_to_buffer(conn, &network, 4);
}

/**
 * @brief 写入 C 字符串
 */
static int write_cstring(PgwireConn *conn, const char *str)
{
    if (!str) {
        return write_byte(conn, '\0');
    }
    int len = strlen(str) + 1;
    return write_to_buffer(conn, str, len);
}

/**
 * @brief 写入字符串（带长度前缀）
 */
static int write_string(PgwireConn *conn, const char *str)
{
    if (!str) {
        return write_int32(conn, -1);
    }
    int len = strlen(str);
    write_int32(conn, len);
    return write_to_buffer(conn, str, len);
}

/* ========================================================================
 * 消息读取
 * ======================================================================== */

/**
 * @brief 确保读取缓冲区有足够数据
 */
static int ensure_read_space(PgwireConn *conn, int needed)
{
    if (conn->read_pos + needed <= conn->read_len) {
        return 0;
    }

    /* 需要读取更多数据 */
    if (conn->read_pos > 0) {
        /* 移动已读取数据到缓冲区开头 */
        memmove(conn->read_buf, conn->read_buf + conn->read_pos,
                conn->read_len - conn->read_pos);
        conn->read_len -= conn->read_pos;
        conn->read_pos = 0;
    } else if (conn->read_len >= conn->read_capacity) {
        /* 扩大缓冲区 */
        int new_capacity = conn->read_capacity * 2;
        char *new_buf = realloc(conn->read_buf, new_capacity);
        if (!new_buf) {
            return -1;
        }
        conn->read_buf = new_buf;
        conn->read_capacity = new_capacity;
    }

    /* 读取数据 */
    int n = read(conn->fd, conn->read_buf + conn->read_len,
                 conn->read_capacity - conn->read_len);
    if (n <= 0) {
        return -1;
    }
    conn->read_len += n;

    if (conn->read_pos + needed > conn->read_len) {
        return ensure_read_space(conn, needed);
    }

    return 0;
}

/**
 * @brief 读取一个字节
 */
static int read_byte(PgwireConn *conn)
{
    if (ensure_read_space(conn, 1) < 0) {
        return -1;
    }
    char byte = conn->read_buf[conn->read_pos++];
    return (unsigned char)byte;
}

/**
 * @brief 读取 int16
 */
static int16_t read_int16(PgwireConn *conn)
{
    if (ensure_read_space(conn, 2) < 0) {
        return -1;
    }
    int16_t val;
    memcpy(&val, conn->read_buf + conn->read_pos, 2);
    conn->read_pos += 2;
    return ntohs(val);
}

/**
 * @brief 读取 int32
 */
static int32_t read_int32(PgwireConn *conn)
{
    if (ensure_read_space(conn, 4) < 0) {
        return -1;
    }
    int32_t val;
    memcpy(&val, conn->read_buf + conn->read_pos, 4);
    conn->read_pos += 4;
    return ntohl(val);
}

/**
 * @brief 读取 C 字符串
 */
static char *read_cstring(PgwireConn *conn)
{
    int start = conn->read_pos;
    while (conn->read_pos < conn->read_len &&
           conn->read_buf[conn->read_pos] != '\0') {
        conn->read_pos++;
    }

    if (conn->read_pos >= conn->read_len) {
        conn->read_pos = start;
        return NULL;
    }

    char *str = strndup(conn->read_buf + start,
                        conn->read_pos - start);
    conn->read_pos++;  /* 跳过 '\0' */
    return str;
}

/**
 * @brief 跳过指定字节
 */
static void skip_bytes(PgwireConn *conn, int n)
{
    conn->read_pos += n;
}

/* ========================================================================
 * 消息读取 API
 * ======================================================================== */

int pgwire_read_msg_header(PgwireConn *conn, PgwireMsgHeader *header)
{
    if (ensure_read_space(conn, 1) < 0) {
        return -1;
    }

    header->type = conn->read_buf[conn->read_pos++];
    header->length = read_int32(conn);
    return 0;
}

int pgwire_read_msg_body(PgwireConn *conn, const PgwireMsgHeader *header,
                        void *buf, int len)
{
    if (header->length - 4 < len) {
        return -1;
    }

    if (ensure_read_space(conn, len) < 0) {
        return -1;
    }

    memcpy(buf, conn->read_buf + conn->read_pos, len);
    conn->read_pos += len;
    return 0;
}

int pgwire_read_startup(PgwireConn *conn, PgwireStartupMsg *msg)
{
    msg->protocol_version = read_int32(conn);
    msg->nparams = 0;
    msg->params = NULL;

    /* 读取参数 */
    char *buf = malloc(4096);
    int buflen = 0;

    while (conn->read_pos < conn->read_len) {
        char *key = read_cstring(conn);
        if (!key || key[0] == '\0') {
            free(key);
            break;
        }
        char *value = read_cstring(conn);

        /* 保存参数 */
        int keylen = strlen(key) + 1;
        int valuelen = value ? strlen(value) + 1 : 1;

        buf = realloc(buf, buflen + keylen + valuelen);
        memcpy(buf + buflen, key, keylen);
        buflen += keylen;
        if (value) {
            memcpy(buf + buflen, value, valuelen);
            buflen += valuelen;
        } else {
            buf[buflen] = '\0';
            buflen++;
        }
        free(key);
        free(value);

        msg->nparams++;
    }

    msg->params = buf;
    return 0;
}

int pgwire_read_password(PgwireConn *conn, char *password, int max_len)
{
    if (ensure_read_space(conn, 1) < 0) {
        return -1;
    }

    skip_bytes(conn, 1);  /* 跳过消息类型 */

    int32_t len = read_int32(conn) - 4;
    if (len > max_len - 1) {
        len = max_len - 1;
    }

    if (ensure_read_space(conn, len) < 0) {
        return -1;
    }

    memcpy(password, conn->read_buf + conn->read_pos, len);
    password[len] = '\0';
    conn->read_pos += len;

    return 0;
}

int pgwire_read_query(PgwireConn *conn, char *query, int max_len)
{
    if (ensure_read_space(conn, 1) < 0) {
        return -1;
    }

    skip_bytes(conn, 1);  /* 跳过消息类型 */

    int32_t len = read_int32(conn) - 4;
    if (len > max_len - 1) {
        len = max_len - 1;
    }

    if (ensure_read_space(conn, len) < 0) {
        return -1;
    }

    memcpy(query, conn->read_buf + conn->read_pos, len);
    query[len] = '\0';
    conn->read_pos += len;

    return 0;
}

int pgwire_read_parse(PgwireConn *conn, PgwireParse *msg)
{
    skip_bytes(conn, 1);  /* 跳过消息类型 */
    read_int32(conn);  /* 跳过长度 */

    msg->name = read_cstring(conn);
    msg->query = read_cstring(conn);
    msg->nparams = read_int16(conn);
    msg->param_types = NULL;

    if (msg->nparams > 0) {
        msg->param_types = malloc(msg->nparams * sizeof(int));
        for (int i = 0; i < msg->nparams; i++) {
            msg->param_types[i] = read_int32(conn);
        }
    }

    return 0;
}

int pgwire_read_bind(PgwireConn *conn, PgwireBind *msg)
{
    skip_bytes(conn, 1);
    read_int32(conn);

    msg->portal = read_cstring(conn);
    msg->statement = read_cstring(conn);
    msg->nformats = read_int16(conn);
    msg->formats = NULL;

    if (msg->nformats > 0) {
        msg->formats = malloc(msg->nformats * sizeof(int16_t));
        for (int i = 0; i < msg->nformats; i++) {
            msg->formats[i] = read_int16(conn);
        }
    }

    msg->nvalues = read_int16(conn);
    msg->value_lengths = NULL;
    msg->values = NULL;

    if (msg->nvalues > 0) {
        msg->value_lengths = malloc(msg->nvalues * sizeof(int32_t));
        msg->values = malloc(msg->nvalues * sizeof(char *));

        for (int i = 0; i < msg->nvalues; i++) {
            msg->value_lengths[i] = read_int32(conn);
            if (msg->value_lengths[i] == -1) {
                msg->values[i] = NULL;  /* NULL 值 */
            } else {
                msg->values[i] = malloc(msg->value_lengths[i]);
                memcpy(msg->values[i],
                       conn->read_buf + conn->read_pos,
                       msg->value_lengths[i]);
                conn->read_pos += msg->value_lengths[i];
            }
        }
    }

    msg->nresult_formats = read_int16(conn);
    msg->result_formats = NULL;

    if (msg->nresult_formats > 0) {
        msg->result_formats = malloc(msg->nresult_formats * sizeof(int16_t));
        for (int i = 0; i < msg->nresult_formats; i++) {
            msg->result_formats[i] = read_int16(conn);
        }
    }

    return 0;
}

int pgwire_read_execute(PgwireConn *conn, PgwireExecute *msg)
{
    skip_bytes(conn, 1);
    read_int32(conn);

    msg->portal = read_cstring(conn);
    msg->max_rows = read_int32(conn);

    return 0;
}

/* ========================================================================
 * 消息写入 API
 * ======================================================================== */

int pgwire_write_auth(PgwireConn *conn, int authtype, const char *data)
{
    write_byte(conn, PGWIRE_MSG_AUTHENTICATE);
    int32_t len = 4;  /* int32_t length */
    if (data) {
        len += strlen(data) + 1;
    }
    write_int32(conn, len);
    write_int32(conn, authtype);
    if (data) {
        write_cstring(conn, data);
    }

    return pgwire_flush(conn);
}

int pgwire_write_error(PgwireConn *conn, const PgwireError *error)
{
    write_byte(conn, PGWIRE_MSG_ERROR_RESPONSE);

    int32_t start_pos = conn->write_pos;
    write_int32(conn, 0);  /* 占位长度 */

    write_byte(conn, 'S');
    write_cstring(conn, error->sqlstate ? error->sqlstate : "ERROR");
    write_byte(conn, 'M');
    write_cstring(conn, error->message ? error->message : "Unknown error");

    if (error->detail) {
        write_byte(conn, 'D');
        write_cstring(conn, error->detail);
    }
    if (error->hint) {
        write_byte(conn, 'H');
        write_cstring(conn, error->hint);
    }
    if (error->file) {
        write_byte(conn, 'F');
        write_cstring(conn, error->file);
    }
    if (error->line > 0) {
        write_byte(conn, 'L');
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", error->line);
        write_cstring(conn, buf);
    }
    if (error->routine) {
        write_byte(conn, 'R');
        write_cstring(conn, error->routine);
    }

    write_byte(conn, '\0');  /* 消息结束 */

    /* 回填长度 */
    int32_t total_len = conn->write_pos - start_pos - 4;
    memcpy(conn->write_buf + start_pos, &total_len, 4);

    return pgwire_flush(conn);
}

int pgwire_write_row_desc(PgwireConn *conn, const PgwireRowDesc *desc)
{
    write_byte(conn, PGWIRE_MSG_ROW_DESCRIPTION);
    int32_t start_pos = conn->write_pos;
    write_int32(conn, 0);

    write_int16(conn, desc->nfields);
    for (int i = 0; i < desc->nfields; i++) {
        write_cstring(conn, desc->fields[i].name);
        write_int32(conn, desc->fields[i].table_oid);
        write_int16(conn, desc->fields[i].column_oid);
        write_int32(conn, desc->fields[i].type_oid);
        write_int16(conn, desc->fields[i].type_len);
        write_int32(conn, desc->fields[i].type_mod);
        write_int16(conn, desc->fields[i].format);
    }

    int32_t total_len = conn->write_pos - start_pos - 4;
    memcpy(conn->write_buf + start_pos, &total_len, 4);

    return pgwire_flush(conn);
}

int pgwire_write_data_row(PgwireConn *conn, const PgwireDataRow *row)
{
    write_byte(conn, PGWIRE_MSG_DATA_ROW);
    int32_t start_pos = conn->write_pos;
    write_int32(conn, 0);

    write_int16(conn, row->nfields);
    for (int i = 0; i < row->nfields; i++) {
        if (row->values[i] == NULL) {
            write_int32(conn, -1);
        } else {
            write_int32(conn, row->lengths[i]);
            write_to_buffer(conn, row->values[i], row->lengths[i]);
        }
    }

    int32_t total_len = conn->write_pos - start_pos - 4;
    memcpy(conn->write_buf + start_pos, &total_len, 4);

    return 0;  /* 不在这里 flush */
}

int pgwire_write_command_complete(PgwireConn *conn, const char *tag)
{
    write_byte(conn, PGWIRE_MSG_COMMAND_COMPLETE);
    write_int32(conn, strlen(tag) + 5);
    write_cstring(conn, tag);

    return pgwire_flush(conn);
}

int pgwire_write_ready(PgwireConn *conn, PgwireTxnState state)
{
    write_byte(conn, PGWIRE_MSG_READY_FOR_QUERY);
    write_int32(conn, 1);
    switch (state) {
        case PGWIRE_TXN_IDLE:
            write_byte(conn, 'I');
            break;
        case PGWIRE_TXN_BLOCK:
            write_byte(conn, 'T');
            break;
        case PGWIRE_TXN_ERR:
            write_byte(conn, 'E');
            break;
    }

    return pgwire_flush(conn);
}

int pgwire_write_parse_complete(PgwireConn *conn)
{
    write_byte(conn, PGWIRE_MSG_PARSE_COMPLETE);
    write_int32(conn, 4);
    return pgwire_flush(conn);
}

int pgwire_write_bind_complete(PgwireConn *conn)
{
    write_byte(conn, PGWIRE_MSG_BIND_COMPLETE);
    write_int32(conn, 4);
    return pgwire_flush(conn);
}

int pgwire_write_close_complete(PgwireConn *conn)
{
    write_byte(conn, PGWIRE_MSG_CLOSE_COMPLETE);
    write_int32(conn, 4);
    return pgwire_flush(conn);
}

int pgwire_write_param_status(PgwireConn *conn, const char *name,
                             const char *value)
{
    write_byte(conn, 'S');
    write_cstring(conn, name);
    write_cstring(conn, value);
    return 0;
}

int pgwire_write_backend_key(PgwireConn *conn, int32_t backend_key,
                            int32_t secret_key)
{
    write_byte(conn, PGWIRE_MSG_BACKEND_KEY_DATA);
    write_int32(conn, 12);
    write_int32(conn, backend_key);
    write_int32(conn, secret_key);

    return pgwire_flush(conn);
}

int pgwire_flush(PgwireConn *conn)
{
    if (conn->write_pos == 0) {
        return 0;
    }

    int total_sent = 0;
    while (total_sent < conn->write_pos) {
        int n = write(conn->fd, conn->write_buf + total_sent,
                      conn->write_pos - total_sent);
        if (n < 0) {
            return -1;
        }
        total_sent += n;
    }

    conn->write_pos = 0;
    return 0;
}

/* ========================================================================
 * 连接管理
 * ======================================================================== */

#define READ_BUF_SIZE 8192
#define WRITE_BUF_SIZE 8192
#define MAX_PORTALS 64
#define MAX_STMTS 64

PgwireConn *pgwire_conn_create(int fd)
{
    PgwireConn *conn = calloc(1, sizeof(PgwireConn));
    if (!conn) return NULL;

    conn->fd = fd;
    conn->state = PGWIRE_CONN_IDLE;
    conn->txn_state = PGWIRE_TXN_IDLE;
    conn->query_state = PGWIRE_QUERY_IDLE;

    conn->read_buf = malloc(READ_BUF_SIZE);
    conn->read_capacity = READ_BUF_SIZE;
    conn->read_pos = 0;
    conn->read_len = 0;

    conn->write_buf = malloc(WRITE_BUF_SIZE);
    conn->write_capacity = WRITE_BUF_SIZE;
    conn->write_pos = 0;

    conn->portals = calloc(MAX_PORTALS, sizeof(PgwirePortal));
    conn->nportals = 0;
    conn->max_portals = MAX_PORTALS;

    conn->stmts = calloc(MAX_STMTS, sizeof(PgwirePreparedStmt));
    conn->nstmts = 0;
    conn->max_stmts = MAX_STMTS;

    conn->backend_key = 0;
    conn->secret_key = rand();

    return conn;
}

void pgwire_conn_destroy(PgwireConn *conn)
{
    if (!conn) return;

    if (conn->fd >= 0) {
        close(conn->fd);
    }

    if (conn->read_buf) free(conn->read_buf);
    if (conn->write_buf) free(conn->write_buf);
    if (conn->portals) free(conn->portals);
    if (conn->stmts) free(conn->stmts);

    free(conn);
}

void pgwire_conn_reset(PgwireConn *conn)
{
    /* 关闭所有 Portal */
    for (int i = 0; i < conn->nportals; i++) {
        /* 清理 Portal 资源 */
    }
    conn->nportals = 0;

    /* 关闭所有预处理语句 */
    for (int i = 0; i < conn->nstmts; i++) {
        conn->stmts[i].valid = false;
    }

    conn->state = PGWIRE_CONN_READY;
    conn->txn_state = PGWIRE_TXN_IDLE;
    conn->query_state = PGWIRE_QUERY_IDLE;
    conn->in_error = false;
}

const char *pgwire_conn_error(PgwireConn *conn)
{
    return conn->error_msg;
}

void pgwire_conn_set_error(PgwireConn *conn, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(conn->error_msg, sizeof(conn->error_msg), fmt, args);
    va_end(args);
    conn->in_error = true;
}

/* ========================================================================
 * 会话管理
 * ======================================================================== */

static int64_t g_session_id_counter = 0;

PgwireSession *pgwire_session_create(PgwireConn *conn)
{
    PgwireSession *session = calloc(1, sizeof(PgwireSession));
    if (!session) return NULL;

    session->session_id = ++g_session_id_counter;
    session->conn = conn;
    session->is_autocommit = true;
    session->xact_start_time = 0;

    return session;
}

void pgwire_session_destroy(PgwireSession *session)
{
    if (!session) return;

    /* 清理事务上下文 */
    /* TODO: 清理资源 */

    free(session);
}

void pgwire_session_set_param(PgwireSession *session, const char *name,
                              const char *value)
{
    /* TODO: 设置会话参数 */
    (void)session;
    (void)name;
    (void)value;
}

const char *pgwire_session_get_param(PgwireSession *session,
                                     const char *name)
{
    /* TODO: 获取会话参数 */
    (void)session;
    (void)name;
    return NULL;
}

/* ========================================================================
 * Portal 和 Prepared Statement
 * ======================================================================== */

PgwirePortal *pgwire_portal_create(PgwireConn *conn, const char *name)
{
    if (conn->nportals >= conn->max_portals) {
        return NULL;
    }

    PgwirePortal *portal = &conn->portals[conn->nportals++];
    strncpy(portal->name, name ? name : "", sizeof(portal->name) - 1);
    portal->stmt = NULL;
    portal->tuple_store = NULL;
    portal->cursor_pos = 0;
    portal->finished = false;

    return portal;
}

PgwirePortal *pgwire_portal_get(PgwireConn *conn, const char *name)
{
    for (int i = 0; i < conn->nportals; i++) {
        if (strcmp(conn->portals[i].name, name == NULL ? "" : name) == 0) {
            return &conn->portals[i];
        }
    }
    return NULL;
}

void pgwire_portal_close(PgwireConn *conn, const char *name)
{
    for (int i = 0; i < conn->nportals; i++) {
        if (strcmp(conn->portals[i].name, name == NULL ? "" : name) == 0) {
            /* 移动后面的 Portal */
            memmove(&conn->portals[i], &conn->portals[i + 1],
                    (conn->nportals - i - 1) * sizeof(PgwirePortal));
            conn->nportals--;
            return;
        }
    }
}

PgwirePreparedStmt *pgwire_stmt_create(PgwireConn *conn, const char *name)
{
    if (conn->nstmts >= conn->max_stmts) {
        return NULL;
    }

    PgwirePreparedStmt *stmt = &conn->stmts[conn->nstmts++];
    strncpy(stmt->name, name ? name : "", sizeof(stmt->name) - 1);
    stmt->nparams = 0;
    stmt->param_types = NULL;
    stmt->plan = NULL;
    stmt->valid = true;

    return stmt;
}

PgwirePreparedStmt *pgwire_stmt_get(PgwireConn *conn, const char *name)
{
    for (int i = 0; i < conn->nstmts; i++) {
        if (strcmp(conn->stmts[i].name, name == NULL ? "" : name) == 0) {
            return &conn->stmts[i];
        }
    }
    return NULL;
}

void pgwire_stmt_close(PgwireConn *conn, const char *name)
{
    for (int i = 0; i < conn->nstmts; i++) {
        if (strcmp(conn->stmts[i].name, name == NULL ? "" : name) == 0) {
            memmove(&conn->stmts[i], &conn->stmts[i + 1],
                    (conn->nstmts - i - 1) * sizeof(PgwirePreparedStmt));
            conn->nstmts--;
            return;
        }
    }
}

/* ========================================================================
 * 类型系统
 * ======================================================================== */

const PgTypeInfo *pgwire_get_type_info(int oid)
{
    for (int i = 0; g_type_info[i].name != NULL; i++) {
        if (g_type_info[i].oid == oid) {
            return &g_type_info[i];
        }
    }
    return NULL;
}

int pgwire_lookup_type(const char *name)
{
    for (int i = 0; g_type_info[i].name != NULL; i++) {
        if (strcasecmp(g_type_info[i].name, name) == 0) {
            return g_type_info[i].oid;
        }
    }
    return 0;
}

int pgwire_encode_binary(int oid, const char *value, char *buf, int buflen)
{
    const PgTypeInfo *info = pgwire_get_type_info(oid);
    if (!info) return -1;

    switch (oid) {
        case PGTYPE_BOOL:
            if (buflen < 1) return -1;
            buf[0] = (strcmp(value, "t") == 0 || strcmp(value, "true") == 0 ||
                     strcmp(value, "1") == 0) ? 1 : 0;
            return 1;

        case PGTYPE_INT2:
            if (buflen < 2) return -1;
            int16_t v2 = htons(atoi(value));
            memcpy(buf, &v2, 2);
            return 2;

        case PGTYPE_INT4:
            if (buflen < 4) return -1;
            int32_t v4 = htonl(atoi(value));
            memcpy(buf, &v4, 4);
            return 4;

        case PGTYPE_INT8:
            if (buflen < 8) return -1;
            int64_t v8 = htobe64(atoll(value));
            memcpy(buf, &v8, 8);
            return 8;

        case PGTYPE_FLOAT4:
            if (buflen < 4) return -1;
            float f4;
            memcpy(&f4, value, sizeof(float));
            /* 转换字节序 */
            char tmp4[4];
            memcpy(tmp4, &f4, 4);
            buf[0] = tmp4[3];
            buf[1] = tmp4[2];
            buf[2] = tmp4[1];
            buf[3] = tmp4[0];
            return 4;

        case PGTYPE_FLOAT8:
            if (buflen < 8) return -1;
            double f8;
            memcpy(&f8, value, sizeof(double));
            /* 转换字节序 */
            char tmp8[8];
            memcpy(tmp8, &f8, 8);
            for (int i = 0; i < 8; i++) {
                buf[i] = tmp8[7 - i];
            }
            return 8;

        default:
            /* 对于变长类型，直接复制字符串 */
            int len = strlen(value);
            if (buflen < len) return -1;
            memcpy(buf, value, len);
            return len;
    }
}

int pgwire_decode_binary(int oid, const char *buf, int buflen, char *value,
                        int maxlen)
{
    switch (oid) {
        case PGTYPE_BOOL:
            snprintf(value, maxlen, "%s", buf[0] ? "t" : "f");
            return 1;

        case PGTYPE_INT2:
            if (buflen < 2) return -1;
            int16_t v2;
            memcpy(&v2, buf, 2);
            snprintf(value, maxlen, "%d", ntohs(v2));
            return 2;

        case PGTYPE_INT4:
            if (buflen < 4) return -1;
            int32_t v4;
            memcpy(&v4, buf, 4);
            snprintf(value, maxlen, "%d", ntohl(v4));
            return 4;

        case PGTYPE_INT8:
            if (buflen < 8) return -1;
            int64_t v8;
            memcpy(&v8, buf, 8);
            snprintf(value, maxlen, "%lld", (long long)be64toh(v8));
            return 8;

        default:
            if (buflen >= maxlen) {
                buflen = maxlen - 1;
            }
            memcpy(value, buf, buflen);
            value[buflen] = '\0';
            return buflen;
    }
}

int pgwire_encode_text(int oid, const char *value, char *buf, int buflen)
{
    /* 文本格式直接复制 */
    int len = strlen(value);
    if (buflen <= len) {
        return -1;
    }
    memcpy(buf, value, len);
    buf[len] = '\0';
    return len + 1;
}

/* ========================================================================
 * 工具函数
 * ======================================================================== */

int pgwire_set_nonblocking(int fd, bool nonblocking)
{
#ifdef _WIN32
    u_long mode = nonblocking ? 1 : 0;
    return ioctlsocket((SOCKET)fd, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }

    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    return fcntl(fd, F_SETFL, flags);
#endif
}

int pgwire_read_full(int fd, void *buf, int len)
{
    int total = 0;
    while (total < len) {
        int n = read(fd, (char *)buf + total, len - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return total;  /* 连接关闭 */
        }
        total += n;
    }
    return total;
}

int pgwire_write_full(int fd, const void *buf, int len)
{
    int total = 0;
    while (total < len) {
        int n = write(fd, (const char *)buf + total, len - total);
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

PgwireError *pgwire_error_create(const char *sqlstate, const char *message)
{
    PgwireError *error = calloc(1, sizeof(PgwireError));
    if (!error) return NULL;

    error->sqlstate = sqlstate ? strdup(sqlstate) : NULL;
    error->message = message ? strdup(message) : NULL;

    return error;
}

void pgwire_error_destroy(PgwireError *error)
{
    if (!error) return;
    if (error->sqlstate) free((char *)error->sqlstate);
    if (error->message) free((char *)error->message);
    if (error->detail) free((char *)error->detail);
    if (error->hint) free((char *)error->hint);
    if (error->file) free((char *)error->file);
    if (error->routine) free((char *)error->routine);
    free(error);
}

/* ========================================================================
 * 服务器 API
 * ======================================================================== */

#define MAX_CONNECTIONS 256

PgwireServer *pgwire_server_create(const PgwireServerConfig *config)
{
    PgwireServer *server = calloc(1, sizeof(PgwireServer));
    if (!server) return NULL;

    if (config) {
        server->config = *config;
    } else {
        server->config.port = 5432;
        server->config.max_connections = 100;
        server->config.auth_method = PGWIRE_AUTH_TRUST;
    }

    server->connections = calloc(MAX_CONNECTIONS, sizeof(PgwireConn *));
    server->max_connections = MAX_CONNECTIONS;

    return server;
}

void pgwire_server_destroy(PgwireServer *server)
{
    if (!server) return;

    /* 关闭所有连接 */
    for (int i = 0; i < server->nconnections; i++) {
        if (server->connections[i]) {
            pgwire_conn_destroy(server->connections[i]);
        }
    }

    if (server->connections) free(server->connections);
    close(server->listen_fd);
    free(server);
}

void pgwire_server_set_auth_callback(PgwireServer *server,
                                    PgwireAuthCallback callback)
{
    server->auth_cb = callback;
}

void pgwire_server_set_exec_callback(PgwireServer *server,
                                    PgwireExecCallback callback)
{
    server->exec_cb = callback;
}

void pgwire_server_set_param_callback(PgwireServer *server,
                                      PgwireParamCallback callback)
{
    server->param_cb = callback;
}

int pgwire_server_start(PgwireServer *server)
{
    struct sockaddr_in addr;

    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        return -1;
    }

    const char opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->config.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server->listen_fd);
        return -1;
    }

    if (listen(server->listen_fd, 128) < 0) {
        close(server->listen_fd);
        return -1;
    }

    return 0;
}

void pgwire_server_stop(PgwireServer *server)
{
    if (server && server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }
}

PgwireConn *pgwire_server_accept(PgwireServer *server)
{
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    int fd = accept(server->listen_fd, (struct sockaddr *)&client_addr,
                    &addrlen);
    if (fd < 0) {
        return NULL;
    }

    PgwireConn *conn = pgwire_conn_create(fd);
    if (!conn) {
        close(fd);
        return NULL;
    }

    /* 保存客户端信息 */
    conn->client.client_addr = strdup(inet_ntoa(client_addr.sin_addr));
    conn->client.client_port = ntohs(client_addr.sin_port);

    return conn;
}

int pgwire_server_poll(PgwireServer *server, int timeout_ms)
{
    /* 简化实现：只接受连接 */
    (void)server;
    (void)timeout_ms;
    return 0;
}

void pgwire_server_get_stats(PgwireServer *server, int *nconnections,
                            int *nqueries)
{
    if (nconnections) *nconnections = server->nconnections;
    if (nqueries) *nqueries = 0;  /* TODO: 统计查询数 */
}

/* ========================================================================
 * 处理客户端请求（简化实现）
 * ======================================================================== */

int pgwire_conn_process(PgwireConn *conn)
{
    PgwireMsgHeader header;

    while (conn->state != PGWIRE_CONN_ERROR &&
           conn->state != PGWIRE_CONN_IDLE) {

        /* 读取消息头 */
        if (pgwire_read_msg_header(conn, &header) < 0) {
            return -1;
        }

        switch (header.type) {
            case PGWIRE_MSG_QUERY:
                {
                    char query[8192];
                    pgwire_read_query(conn, query, sizeof(query));

                    /* 发送空查询响应：使用 PGWIRE_MSG_EMPTY_QUERY 类型 */
                    write_byte(conn, PGWIRE_MSG_EMPTY_QUERY);
                    write_int32(conn, 4);

                    /* 发送就绪状态 */
                    pgwire_write_ready(conn, conn->txn_state);
                }
                break;

            case PGWIRE_MSG_TERMINATE:
                conn->state = PGWIRE_CONN_IDLE;
                return -1;

            default:
                /* 跳过未知消息 */
                skip_bytes(conn, header.length - 4);
                break;
        }
    }

    return 0;
}
