/**
 * @file pg_wire_protocol.c
 * @brief PostgreSQL Wire Protocol 兼容层实现
 *
 * 实现 PostgreSQL v3.0 wire 协议的消息处理，
 * 包括启动、认证、查询、错误响应等。
 */

#include "db/pg_wire_protocol.h"
#include "db/log.h"
#include "db/errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define close_socket closesocket
#define pg_getpid() GetCurrentProcessId()
#else
#include <sys/socket.h>
#include <unistd.h>
#include <unistd.h>
#define close_socket close
#define pg_getpid() getpid()
#endif

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 写入 32 位整数（网络字节序）
 */
static void write_int32(char *buf, int32_t val) {
    uint32_t uval = (uint32_t)val;
    buf[0] = (char)((uval >> 24) & 0xFF);
    buf[1] = (char)((uval >> 16) & 0xFF);
    buf[2] = (char)((uval >> 8) & 0xFF);
    buf[3] = (char)(uval & 0xFF);
}

/**
 * @brief 写入 16 位整数（网络字节序）
 */
static void write_int16(char *buf, int16_t val) {
    uint16_t uval = (uint16_t)val;
    buf[0] = (char)((uval >> 8) & 0xFF);
    buf[1] = (char)(uval & 0xFF);
}

/**
 * @brief 读取 32 位整数（网络字节序）
 */
static int32_t read_int32(const char *buf) {
    const unsigned char *p = (const unsigned char *)buf;
    return (int32_t)((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

/**
 * @brief 读取 16 位整数（网络字节序）
 */
static int16_t read_int16(const char *buf) {
    const unsigned char *p = (const unsigned char *)buf;
    return (int16_t)((p[0] << 8) | p[1]);
}

/**
 * @brief 发送数据
 */
static int do_send(int sock, const void *data, int len) {
    const char *p = (const char *)data;
    int total = 0;
    while (total < len) {
        int n = (int)send(sock, p + total, len - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

/* ============================================================
 * 连接管理
 * ============================================================ */

pg_connection_t* pg_connection_create(int sock) {
    pg_connection_t *conn = (pg_connection_t *)calloc(1, sizeof(pg_connection_t));
    if (!conn) {
        LOG_ERROR("pg_connection_create: 内存分配失败");
        return NULL;
    }

    conn->sock = sock;
    conn->state = PG_CONN_STATE_STARTUP;
    conn->authenticated = false;
    conn->protocol_version = 0;

    /* 分配读写缓冲区 */
    conn->read_buf_size = 8192;
    conn->read_buf = (char *)malloc(conn->read_buf_size);
    conn->read_buf_len = 0;
    conn->read_buf_pos = 0;

    conn->write_buf_size = 8192;
    conn->write_buf = (char *)malloc(conn->write_buf_size);
    conn->write_buf_len = 0;

    if (!conn->read_buf || !conn->write_buf) {
        LOG_ERROR("pg_connection_create: 缓冲区分配失败");
        free(conn->read_buf);
        free(conn->write_buf);
        free(conn);
        return NULL;
    }

    /* 默认配置 */
    conn->config.server_encoding = strdup("UTF8");
    conn->config.integer_datetimes = 1;
    conn->config.client_encoding = strdup("UTF8");
    conn->config.database_encoding = strdup("UTF8");
    conn->config.date_style = strdup("ISO, MDY");
    conn->config.interval_style = strdup("postgres");
    conn->config.timezone = strdup("UTC");
    conn->config.application_name = strdup("unknown");

    LOG_INFO("pg_connection_create: sock=%d", sock);
    return conn;
}

void pg_connection_free(pg_connection_t *conn) {
    if (!conn) return;

    LOG_INFO("pg_connection_free: sock=%d queries=%lu", conn->sock, (unsigned long)conn->query_count);

    free(conn->user);
    free(conn->database);
    free(conn->read_buf);
    free(conn->write_buf);
    free(conn->portal_name);
    free(conn->priv_data);
    free(conn->executor_data);

    free(conn->config.server_encoding);
    free(conn->config.client_encoding);
    free(conn->config.database_encoding);
    free(conn->config.date_style);
    free(conn->config.interval_style);
    free(conn->config.timezone);
    free(conn->config.application_name);

    free(conn);
}

int pg_connection_reset(pg_connection_t *conn) {
    if (!conn) return -1;

    /* 清理旧状态 */
    free(conn->user);
    free(conn->database);
    free(conn->portal_name);

    conn->user = NULL;
    conn->database = NULL;
    conn->portal_name = NULL;
    conn->portal_open = false;

    conn->state = PG_CONN_STATE_STARTUP;
    conn->authenticated = false;
    conn->protocol_version = 0;
    conn->query_count = 0;

    conn->read_buf_len = 0;
    conn->read_buf_pos = 0;
    conn->write_buf_len = 0;

    return 0;
}

/* ============================================================
 * 读写操作
 * ============================================================ */

int pg_connection_read(pg_connection_t *conn) {
    if (!conn) return -2;

    /* 如果缓冲区满了，扩展缓冲区 */
    if (conn->read_buf_len >= conn->read_buf_size) {
        int new_size = conn->read_buf_size * 2;
        char *new_buf = (char *)realloc(conn->read_buf, new_size);
        if (!new_buf) return -2;
        conn->read_buf = new_buf;
        conn->read_buf_size = new_size;
    }

    int space = conn->read_buf_size - conn->read_buf_len;
    int n = (int)recv(conn->sock, conn->read_buf + conn->read_buf_len, space, 0);
    if (n <= 0) return -1;  /* 连接关闭或错误 */

    conn->read_buf_len += n;
    conn->bytes_received += n;
    return 0;
}

int pg_connection_flush(pg_connection_t *conn) {
    if (!conn || conn->write_buf_len == 0) return 0;

    int sent = do_send(conn->sock, conn->write_buf, conn->write_buf_len);
    if (sent != 0) return -1;

    conn->bytes_sent += conn->write_buf_len;
    conn->write_buf_len = 0;
    return 0;
}

int pg_connection_write(pg_connection_t *conn, const void *data, int len) {
    if (!conn) return -1;

    /* 确保缓冲区足够 */
    while (conn->write_buf_len + len > conn->write_buf_size) {
        int new_size = conn->write_buf_size * 2;
        char *new_buf = (char *)realloc(conn->write_buf, new_size);
        if (!new_buf) return -1;
        conn->write_buf = new_buf;
        conn->write_buf_size = new_size;
    }

    memcpy(conn->write_buf + conn->write_buf_len, data, len);
    conn->write_buf_len += len;
    return 0;
}

/* ============================================================
 * 协议处理 - 启动消息
 * ============================================================ */

int pg_handle_startup(pg_connection_t *conn, const char *data, int len) {
    if (!conn || len < 8) return -1;

    int32_t version = read_int32(data);
    int major = (version >> 16) & 0xFFFF;
    int minor = version & 0xFFFF;

    LOG_INFO("pg_handle_startup: 版本 %d.%d", major, minor);

    if (major != 3 || minor != 0) {
        LOG_ERROR("pg_handle_startup: 不支持的协议版本 %d.%d", major, minor);
        pg_send_error_response(conn, "FATAL", "08001",
                               "不支持的协议版本，仅支持 3.0");
        pg_connection_flush(conn);
        return -1;
    }

    conn->protocol_version = version;

    /* 解析键值对 */
    const char *p = data + 4;  /* 跳过长度 */
    const char *end = data + len;

    while (p < end) {
        /* 读取 key（null 结尾的字符串） */
        const char *key_start = p;
        while (p < end && *p != '\0') p++;
        if (p >= end) break;
        const char *key = key_start;
        p++;  /* 跳过 null */

        /* 读取 value */
        const char *val_start = p;
        while (p < end && *p != '\0') p++;
        if (p >= end) break;
        const char *value = val_start;
        p++;  /* 跳过 null */

        /* 存储参数 */
        if (strcmp(key, "user") == 0) {
            free(conn->user);
            conn->user = strdup(value);
        } else if (strcmp(key, "database") == 0) {
            free(conn->database);
            conn->database = strdup(value);
        } else if (strcmp(key, "client_encoding") == 0) {
            free(conn->config.client_encoding);
            conn->config.client_encoding = strdup(value);
        } else if (strcmp(key, "application_name") == 0) {
            free(conn->config.application_name);
            conn->config.application_name = strdup(value);
        } else if (strcmp(key, "date_style") == 0) {
            free(conn->config.date_style);
            conn->config.date_style = strdup(value);
        } else if (strcmp(key, "timezone") == 0) {
            free(conn->config.timezone);
            conn->config.timezone = strdup(value);
        }
    }

    /* 如果没有指定数据库，使用用户名作为数据库名 */
    if (!conn->database && conn->user) {
        conn->database = strdup(conn->user);
    }

    LOG_INFO("pg_handle_startup: user=%s database=%s",
             conn->user ? conn->user : "(null)",
             conn->database ? conn->database : "(null)");

    /* 发送认证成功 */
    if (pg_send_auth_ok(conn) != 0) return -1;
    if (pg_connection_flush(conn) != 0) return -1;

    conn->state = PG_CONN_STATE_AUTH;

    /* 发送参数状态 */
    pg_send_parameter_status(conn, "server_version", "17.0");
    pg_send_parameter_status(conn, "server_encoding", conn->config.server_encoding);
    pg_send_parameter_status(conn, "client_encoding", conn->config.client_encoding);
    pg_send_parameter_status(conn, "database_encoding", conn->config.database_encoding);
    pg_send_parameter_status(conn, "integer_datetimes", "1");
    pg_send_parameter_status(conn, "is_superuser", "on");
    pg_send_parameter_status(conn, "session_authorization", conn->user);
    pg_send_parameter_status(conn, "DateStyle", conn->config.date_style);
    pg_send_parameter_status(conn, "IntervalStyle", conn->config.interval_style);
    pg_send_parameter_status(conn, "TimeZone", conn->config.timezone);
    pg_send_parameter_status(conn, "standard_conforming_strings", "on");
    pg_send_parameter_status(conn, "application_name", conn->config.application_name);

    /* 发送后端密钥数据（简化：使用 sock 作为 key） */
    pg_send_backend_key_data(conn, (int32_t)pg_getpid(), conn->sock);

    /* 发送 ReadyForQuery */
    pg_send_ready_for_query(conn);

    if (pg_connection_flush(conn) != 0) return -1;

    conn->state = PG_CONN_STATE_READY;
    return 0;
}

int pg_handle_password(pg_connection_t *conn, const char *data, int len) {
    /* 简化实现：接受任何密码（信任认证） */
    (void)data;
    (void)len;

    if (!conn) return -1;

    /* 发送认证成功 */
    pg_send_auth_ok(conn);
    pg_send_ready_for_query(conn);

    conn->state = PG_CONN_STATE_READY;
    conn->authenticated = true;

    return pg_connection_flush(conn);
}

/* ============================================================
 * 协议处理 - 简单查询
 * ============================================================ */

int pg_handle_query(pg_connection_t *conn, const char *query) {
    if (!conn || !query) return -1;

    LOG_INFO("pg_handle_query: %s", query);

    conn->query_count++;
    conn->state = PG_CONN_STATE_QUERY;

    /* 调用执行器（这里先用 stub 实现） */
    /* TODO: 接入实际的 SQL 执行器 */

    /* 发送空结果（stub） */
    pg_send_empty_query_response(conn);
    pg_send_ready_for_query(conn);

    conn->state = PG_CONN_STATE_READY;
    return pg_connection_flush(conn);
}

int pg_handle_terminate(pg_connection_t *conn) {
    if (!conn) return 0;

    LOG_INFO("pg_handle_terminate: 客户端请求终止");
    conn->state = PG_CONN_STATE_TERMINATED;
    close_socket(conn->sock);
    return 0;
}

/* ============================================================
 * 协议处理 - 扩展查询
 * ============================================================ */

int pg_handle_parse(pg_connection_t *conn, const char *stmt_name,
                    const char *query, int num_params, const int32_t *param_oids) {
    if (!conn || !query) return -1;

    LOG_INFO("pg_handle_parse: stmt=%s query=%s",
             stmt_name ? stmt_name : "(unnamed)", query);

    conn->query_count++;

    /* TODO: 实际解析查询 */

    /* 发送 ParseComplete */
    pg_send_parse_complete(conn);
    return pg_connection_flush(conn);
}

int pg_handle_bind(pg_connection_t *conn, const char *portal_name,
                   const char *stmt_name, int num_params,
                   const int16_t *param_formats,
                   const char *const *param_values,
                   const int32_t *param_lens,
                   int result_format) {
    if (!conn) return -1;

    LOG_INFO("pg_handle_bind: portal=%s stmt=%s",
             portal_name ? portal_name : "(unnamed)",
             stmt_name ? stmt_name : "(unnamed)");

    /* 存储门户名称 */
    free(conn->portal_name);
    conn->portal_name = portal_name ? strdup(portal_name) : strdup("");

    /* 发送 BindComplete */
    pg_send_bind_complete(conn);
    return pg_connection_flush(conn);
}

int pg_handle_execute(pg_connection_t *conn, const char *portal_name, int max_rows) {
    if (!conn) return -1;

    LOG_INFO("pg_handle_execute: portal=%s max_rows=%d",
             portal_name ? portal_name : "(unnamed)", max_rows);

    /* TODO: 实际执行查询 */

    /* 发送 CommandComplete + ReadyForQuery */
    pg_send_command_complete(conn, "SELECT 0");
    pg_send_ready_for_query(conn);

    return pg_connection_flush(conn);
}

int pg_send_no_data(pg_connection_t *conn) {
    if (!conn) return -1;

    char msg[5];
    msg[0] = 'n';  /* NoData */
    write_int32(msg + 1, 4);

    return pg_connection_write(conn, msg, 5);
}

int pg_handle_describe(pg_connection_t *conn, char target_type, const char *name) {
    if (!conn) return -1;

    LOG_INFO("pg_handle_describe: type=%c name=%s", target_type, name ? name : "(null)");

    /* TODO: 实际描述语句/门户 */

    /* 发送 NoData（stub） */
    pg_send_no_data(conn);
    return pg_connection_flush(conn);
}

int pg_handle_sync(pg_connection_t *conn) {
    if (!conn) return -1;

    LOG_INFO("pg_handle_sync");

    /* 发送 ReadyForQuery */
    pg_send_ready_for_query(conn);
    return pg_connection_flush(conn);
}

int pg_handle_close(pg_connection_t *conn, char target_type, const char *name) {
    if (!conn) return -1;

    LOG_INFO("pg_handle_close: type=%c name=%s", target_type, name ? name : "(null)");

    /* 发送 CloseComplete */
    pg_send_close_complete(conn);
    return pg_connection_flush(conn);
}

/* ============================================================
 * 响应发送
 * ============================================================ */

int pg_send_auth_ok(pg_connection_t *conn) {
    if (!conn) return -1;

    char msg[9];
    msg[0] = 'R';  /* AuthenticationOk */
    write_int32(msg + 1, 8);  /* 长度 = 8（含自身） */
    write_int32(msg + 5, PG_AUTH_OK);

    return pg_connection_write(conn, msg, 9);
}

int pg_send_parameter_status(pg_connection_t *conn, const char *name, const char *value) {
    if (!conn || !name || !value) return -1;

    int name_len = (int)strlen(name);
    int value_len = (int)strlen(value);
    int msg_len = 4 + name_len + 1 + value_len + 1;  /* 长度 + name\0 + value\0 */

    char *msg = (char *)malloc(msg_len);
    if (!msg) return -1;

    msg[0] = 'K';  /* ParameterStatus */
    write_int32(msg + 1, msg_len);
    memcpy(msg + 5, name, name_len + 1);
    memcpy(msg + 5 + name_len + 1, value, value_len + 1);

    int ret = pg_connection_write(conn, msg, msg_len);
    free(msg);
    return ret;
}

int pg_send_backend_key_data(pg_connection_t *conn, int32_t pid, int32_t key) {
    if (!conn) return -1;

    char msg[13];
    msg[0] = 'k';  /* BackendKeyData */
    write_int32(msg + 1, 12);
    write_int32(msg + 5, pid);
    write_int32(msg + 9, key);

    return pg_connection_write(conn, msg, 13);
}

int pg_send_ready_for_query(pg_connection_t *conn) {
    if (!conn) return -1;

    char status;
    switch (conn->state) {
        case PG_CONN_STATE_READY:  status = PG_TXN_IDLE; break;
        case PG_CONN_STATE_QUERY:  status = PG_TXN_IN_BLOCK; break;
        default:                   status = PG_TXN_IDLE; break;
    }

    char msg[6];
    msg[0] = 'Z';  /* ReadyForQuery */
    write_int32(msg + 1, 5);
    msg[4] = status;

    return pg_connection_write(conn, msg, 5);
}

int pg_send_error_response(pg_connection_t *conn, const char *severity,
                           const char *sqlstate, const char *message) {
    if (!conn || !severity || !sqlstate || !message) return -1;

    int sev_len = (int)strlen(severity);
    int sql_len = (int)strlen(sqlstate);
    int msg_len = (int)strlen(message);

    /* 构建消息：type + length + S\0severity\0 + C\0sqlstate\0 + M\0message\0 + \0 */
    int total_len = 4 + 1 + sev_len + 1 + 1 + sql_len + 1 + 1 + msg_len + 1 + 1;

    char *buf = (char *)malloc(total_len);
    if (!buf) return -1;

    int pos = 0;
    buf[pos++] = 'E';  /* ErrorResponse */
    write_int32(buf + pos, total_len); pos += 4;

    /* Severity */
    buf[pos++] = 'S';
    memcpy(buf + pos, severity, sev_len + 1); pos += sev_len + 1;

    /* SQLSTATE */
    buf[pos++] = 'C';
    memcpy(buf + pos, sqlstate, sql_len + 1); pos += sql_len + 1;

    /* Message */
    buf[pos++] = 'M';
    memcpy(buf + pos, message, msg_len + 1); pos += msg_len + 1;

    /* 终止符 */
    buf[pos++] = '\0';

    int ret = pg_connection_write(conn, buf, pos);
    free(buf);
    return ret;
}

int pg_send_row_description(pg_connection_t *conn, const pg_column_desc_t *columns,
                            int num_columns) {
    if (!conn) return -1;

    /* 计算消息长度 */
    int msg_len = 4 + 2;  /* 长度 + 列数 */
    for (int i = 0; i < num_columns; i++) {
        msg_len += (int)strlen(columns[i].name) + 1;  /* 列名 + null */
        msg_len += 4 + 2 + 4 + 2 + 4 + 2;  /* table_oid + column_attr + type_oid + type_len + type_mod + format */
    }

    char *buf = (char *)malloc(msg_len);
    if (!buf) return -1;

    int pos = 0;
    buf[pos++] = 'T';  /* RowDescription */
    write_int32(buf + pos, msg_len); pos += 4;
    write_int16(buf + pos, (int16_t)num_columns); pos += 2;

    for (int i = 0; i < num_columns; i++) {
        const pg_column_desc_t *col = &columns[i];

        /* 列名 */
        int name_len = (int)strlen(col->name);
        memcpy(buf + pos, col->name, name_len + 1); pos += name_len + 1;

        /* Table OID */
        write_int32(buf + pos, col->table_oid); pos += 4;

        /* Column attribute number */
        write_int16(buf + pos, col->column_attr); pos += 2;

        /* Data type OID */
        write_int32(buf + pos, col->type_oid); pos += 4;

        /* Data type length */
        write_int16(buf + pos, col->type_len); pos += 2;

        /* Type modifier */
        write_int32(buf + pos, col->type_mod); pos += 4;

        /* Format code */
        write_int16(buf + pos, col->format_code); pos += 2;
    }

    int ret = pg_connection_write(conn, buf, msg_len);
    free(buf);
    return ret;
}

int pg_send_data_row(pg_connection_t *conn, const char *const *values,
                     const int32_t *value_lens, int num_columns) {
    if (!conn) return -1;

    /* 计算消息长度 */
    int msg_len = 4 + 2;  /* 长度 + 列数 */
    for (int i = 0; i < num_columns; i++) {
        msg_len += 4;  /* 列长度 */
        if (value_lens[i] >= 0 && values[i]) {
            msg_len += value_lens[i];
        }
    }

    char *buf = (char *)malloc(msg_len);
    if (!buf) return -1;

    int pos = 0;
    buf[pos++] = 'D';  /* DataRow */
    write_int32(buf + pos, msg_len); pos += 4;
    write_int16(buf + pos, (int16_t)num_columns); pos += 2;

    for (int i = 0; i < num_columns; i++) {
        if (value_lens[i] < 0 || !values[i]) {
            /* NULL */
            write_int32(buf + pos, -1); pos += 4;
        } else {
            write_int32(buf + pos, value_lens[i]); pos += 4;
            memcpy(buf + pos, values[i], value_lens[i]); pos += value_lens[i];
        }
    }

    int ret = pg_connection_write(conn, buf, msg_len);
    free(buf);
    return ret;
}

int pg_send_command_complete(pg_connection_t *conn, const char *tag) {
    if (!conn || !tag) return -1;

    int tag_len = (int)strlen(tag);
    int msg_len = 4 + tag_len + 1;  /* 长度 + tag + null */

    char *buf = (char *)malloc(msg_len);
    if (!buf) return -1;

    buf[0] = 'C';  /* CommandComplete */
    write_int32(buf + 1, msg_len);
    memcpy(buf + 5, tag, tag_len + 1);

    int ret = pg_connection_write(conn, buf, msg_len);
    free(buf);
    return ret;
}

int pg_send_empty_query_response(pg_connection_t *conn) {
    if (!conn) return -1;

    char msg[5];
    msg[0] = 'I';  /* EmptyQueryResponse */
    write_int32(msg + 1, 4);

    return pg_connection_write(conn, msg, 5);
}

int pg_send_parse_complete(pg_connection_t *conn) {
    if (!conn) return -1;

    char msg[5];
    msg[0] = '1';  /* ParseComplete */
    write_int32(msg + 1, 4);

    return pg_connection_write(conn, msg, 5);
}

int pg_send_bind_complete(pg_connection_t *conn) {
    if (!conn) return -1;

    char msg[5];
    msg[0] = '2';  /* BindComplete */
    write_int32(msg + 1, 4);

    return pg_connection_write(conn, msg, 5);
}

int pg_send_close_complete(pg_connection_t *conn) {
    if (!conn) return -1;

    char msg[5];
    msg[0] = '3';  /* CloseComplete */
    write_int32(msg + 1, 4);

    return pg_connection_write(conn, msg, 5);
}

/* ============================================================
 * 结果集操作
 * ============================================================ */

pg_result_t* pg_result_create(void) {
    pg_result_t *result = (pg_result_t *)calloc(1, sizeof(pg_result_t));
    if (!result) {
        LOG_ERROR("pg_result_create: 内存分配失败");
        return NULL;
    }
    return result;
}

void pg_result_free(pg_result_t *result) {
    if (!result) return;

    /* 释放列描述 */
    if (result->columns) {
        for (int i = 0; i < result->num_columns; i++) {
            free(result->columns[i].name);
        }
        free(result->columns);
    }

    /* 释放值 */
    if (result->values) {
        for (int i = 0; i < result->num_rows * result->num_columns; i++) {
            free(result->values[i]);
        }
        free(result->values);
    }

    free(result->value_lens);
    free(result->cmd_status);
    free(result->error_message);
    free(result->error_severity);
    free(result->error_sqlstate);
    free(result);
}

int pg_result_set_columns(pg_result_t *result, const pg_column_desc_t *columns,
                          int num_columns) {
    if (!result || !columns || num_columns < 0) return -1;

    /* 释放旧列 */
    if (result->columns) {
        for (int i = 0; i < result->num_columns; i++) {
            free(result->columns[i].name);
        }
        free(result->columns);
    }

    result->num_columns = num_columns;
    result->columns = (pg_column_desc_t *)malloc(sizeof(pg_column_desc_t) * num_columns);
    if (!result->columns && num_columns > 0) return -1;

    for (int i = 0; i < num_columns; i++) {
        result->columns[i] = columns[i];
        result->columns[i].name = strdup(columns[i].name);
    }

    return 0;
}

int pg_result_add_row(pg_result_t *result, const char *const *values,
                      const int32_t *value_lens, int num_columns) {
    if (!result || num_columns != result->num_columns) return -1;

    /* 扩展数组 */
    if (result->num_rows >= result->num_rows_alloc) {
        int new_alloc = result->num_rows_alloc == 0 ? 64 : result->num_rows_alloc * 2;
        char **new_values = (char **)realloc(result->values, sizeof(char *) * new_alloc * num_columns);
        int32_t *new_lens = (int32_t *)realloc(result->value_lens, sizeof(int32_t) * new_alloc * num_columns);

        if (!new_values || !new_lens) {
            free(new_values);
            free(new_lens);
            return -1;
        }

        result->values = new_values;
        result->value_lens = new_lens;
        result->num_rows_alloc = new_alloc;
    }

    /* 拷贝值 */
    int offset = result->num_rows * num_columns;
    for (int i = 0; i < num_columns; i++) {
        if (value_lens[i] < 0 || !values[i]) {
            result->values[offset + i] = NULL;
            result->value_lens[offset + i] = -1;
        } else {
            result->values[offset + i] = (char *)malloc(value_lens[i]);
            if (!result->values[offset + i]) return -1;
            memcpy(result->values[offset + i], values[i], value_lens[i]);
            result->value_lens[offset + i] = value_lens[i];
        }
    }

    result->num_rows++;
    return 0;
}

void pg_result_set_command(pg_result_t *result, const char *tag,
                           int64_t affected, int64_t oid) {
    if (!result) return;
    free(result->cmd_status);
    result->cmd_status = tag ? strdup(tag) : NULL;
    result->cmd_affected = affected;
    result->cmd_oid = oid;
}

void pg_result_set_error(pg_result_t *result, const char *severity,
                         const char *sqlstate, const char *message) {
    if (!result) return;

    result->is_error = true;
    free(result->error_severity);
    free(result->error_sqlstate);
    free(result->error_message);

    result->error_severity = severity ? strdup(severity) : NULL;
    result->error_sqlstate = sqlstate ? strdup(sqlstate) : NULL;
    result->error_message = message ? strdup(message) : NULL;
}

int pg_send_result(pg_connection_t *conn, pg_result_t *result) {
    if (!conn || !result) return -1;

    if (result->is_error) {
        return pg_send_error_response(conn,
            result->error_severity ? result->error_severity : "ERROR",
            result->error_sqlstate ? result->error_sqlstate : "XX000",
            result->error_message ? result->error_message : "未知错误");
    }

    /* 发送 RowDescription */
    if (result->num_columns > 0 && result->columns) {
        pg_send_row_description(conn, result->columns, result->num_columns);
    }

    /* 发送 DataRows */
    for (int r = 0; r < result->num_rows; r++) {
        int offset = r * result->num_columns;
        pg_send_data_row(conn,
            (const char *const *)(result->values + offset),
            result->value_lens + offset,
            result->num_columns);
    }

    /* 发送 CommandComplete */
    if (result->cmd_status) {
        pg_send_command_complete(conn, result->cmd_status);
    }

    return 0;
}

/* ============================================================
 * 工具函数
 * ============================================================ */

char* pg_build_command_tag(char *buf, size_t buf_size, const char *tag,
                           int64_t oid, int64_t affected) {
    if (!buf || buf_size == 0) return buf;

    if (oid > 0) {
        snprintf(buf, buf_size, "%s %ld %ld", tag, (long)affected, (long)oid);
    } else if (affected >= 0) {
        snprintf(buf, buf_size, "%s %ld", tag, (long)affected);
    } else {
        snprintf(buf, buf_size, "%s", tag);
    }

    return buf;
}

int32_t pg_type_oid_from_c_type(const char *c_type) {
    if (!c_type) return PG_TYPE_OID_UNKNOWN;

    if (strcmp(c_type, "bool") == 0 || strcmp(c_type, "_Bool") == 0)
        return PG_TYPE_OID_BOOL;
    if (strcmp(c_type, "int8") == 0 || strcmp(c_type, "int64_t") == 0)
        return PG_TYPE_OID_INT8;
    if (strcmp(c_type, "int2") == 0 || strcmp(c_type, "int16_t") == 0)
        return PG_TYPE_OID_INT2;
    if (strcmp(c_type, "int4") == 0 || strcmp(c_type, "int32_t") == 0)
        return PG_TYPE_OID_INT4;
    if (strcmp(c_type, "float4") == 0 || strcmp(c_type, "float") == 0)
        return PG_TYPE_OID_FLOAT4;
    if (strcmp(c_type, "float8") == 0 || strcmp(c_type, "double") == 0)
        return PG_TYPE_OID_FLOAT8;
    if (strcmp(c_type, "text") == 0 || strcmp(c_type, "char*") == 0 || strcmp(c_type, "string") == 0)
        return PG_TYPE_OID_TEXT;
    if (strcmp(c_type, "bytea") == 0)
        return PG_TYPE_OID_BYTEA;

    return PG_TYPE_OID_UNKNOWN;
}
