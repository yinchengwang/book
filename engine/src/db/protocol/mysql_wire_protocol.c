/**
 * @file mysql_wire_protocol.c
 * @brief MySQL 线协议兼容层实现
 *
 * 实现 MySQL 4.1+ 线协议，处理握手、查询、结果集发送等。
 */
#include "db/mysql_wire_protocol.h"
#include "db/core/log.h"
#include "db/errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

/* MySQL 协议常量 */
#define MYSQL_PROTOCOL_VERSION  10
#define MYSQL_SERVER_VERSION    "8.0.26-MultiModal-1.0"
#define MYSQL_SCRAMBLE_LENGTH   20
#define MYSQL_MAX_PACKET_SIZE   (1 << 24)  /* 16MB */

/* MySQL 标志位 */
#define MYSQL_FLAG_FOUND_ROWS       (1 << 1)
#define MYSQL_FLAG_LONG_FLAG        (1 << 3)
#define MYSQL_FLAG_NOT_NULL         (1 << 4)
#define MYSQL_FLAG_PRIMARY_KEY      (1 << 8)
#define MYSQL_FLAG_UNIQUE_KEY       (1 << 10)
#define MYSQL_FLAG_BLOB             (1 << 15)
#define MYSQL_FLAG_PROTOCOL_41      (1 << 9)
#define MYSQL_FLAG_SECURE_CONNECTION (1 << 11)

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 写 3 字节长度编码（little-endian）
 */
static void write_3byte_length(void *buf, uint32_t len) {
    uint8_t *p = (uint8_t *)buf;
    p[0] = (uint8_t)(len & 0xFF);
    p[1] = (uint8_t)((len >> 8) & 0xFF);
    p[2] = (uint8_t)((len >> 16) & 0xFF);
}

/**
 * @brief 读 3 字节长度编码
 */
static uint32_t read_3byte_length(const void *buf) {
    const uint8_t *p = (const uint8_t *)buf;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

/**
 * @brief 写 little-endian 整数
 */
static void write_int2(void *buf, uint16_t val) {
    uint8_t *p = (uint8_t *)buf;
    p[0] = (uint8_t)(val & 0xFF);
    p[1] = (uint8_t)((val >> 8) & 0xFF);
}

static void write_int4(void *buf, uint32_t val) {
    uint8_t *p = (uint8_t *)buf;
    p[0] = (uint8_t)(val & 0xFF);
    p[1] = (uint8_t)((val >> 8) & 0xFF);
    p[2] = (uint8_t)((val >> 16) & 0xFF);
    p[3] = (uint8_t)((val >> 24) & 0xFF);
}

static void write_int8(void *buf, uint64_t val) {
    uint8_t *p = (uint8_t *)buf;
    for (int i = 0; i < 8; i++) {
        p[i] = (uint8_t)((val >> (i * 8)) & 0xFF);
    }
}

/**
 * @brief 写长度编码字符串（以 0x00 结尾）
 */
static void write_cstring(void *buf, const char *str) {
    if (str) {
        strcpy((char *)buf, str);
    }
}

/**
 * @brief 计算 C 字符串长度（含结尾 0）
 */
static size_t cstring_len(const char *str) {
    return str ? (strlen(str) + 1) : 1;
}

/* ============================================================
 * 连接管理
 * ============================================================ */

mysql_connection_t* mysql_connection_create(int sock) {
    mysql_connection_t *conn = (mysql_connection_t *)calloc(1, sizeof(mysql_connection_t));
    if (!conn) {
        LOG_ERROR("mysql_connection_create: 内存分配失败");
        return NULL;
    }
    conn->sock = sock;
    conn->packet_id = 0;
    conn->authenticated = false;
    LOG_INFO("mysql_connection_create: 创建连接 (fd=%d)", sock);
    return conn;
}

void mysql_connection_free(mysql_connection_t *conn) {
    if (!conn) return;
    LOG_INFO("mysql_connection_free: 释放连接 (fd=%d)", conn->sock);
    if (conn->user) free(conn->user);
    if (conn->database) free(conn->database);
    free(conn);
}

uint32_t mysql_next_packet_id(mysql_connection_t *conn) {
    return conn->packet_id++;
}

/* ============================================================
 * 数据收发
 * ============================================================ */

int mysql_send_raw(mysql_connection_t *conn, const void *data, size_t len) {
    if (!conn || !data) return -1;
    const uint8_t *p = (const uint8_t *)data;
    size_t remaining = len;
    while (remaining > 0) {
#ifdef _WIN32
        ssize_t sent = send(conn->sock, (const char *)p, (int)remaining, 0);
#else
        ssize_t sent = send(conn->sock, p, remaining, 0);
#endif
        if (sent <= 0) {
            LOG_ERROR("mysql_send_raw: 发送失败 (fd=%d)", conn->sock);
            return -1;
        }
        p += sent;
        remaining -= (size_t)sent;
    }
    return 0;
}

int mysql_read_packet(mysql_connection_t *conn, void *buffer, size_t max_len) {
    if (!conn || !buffer) return -1;

    /* 先读 4 字节头（3 字节长度 + 1 字节序号） */
    uint8_t header[4];
    size_t total = 0;
    while (total < 4) {
#ifdef _WIN32
        ssize_t n = recv(conn->sock, (char *)(header + total), (int)(4 - total), 0);
#else
        ssize_t n = recv(conn->sock, header + total, 4 - total, 0);
#endif
        if (n <= 0) {
            LOG_ERROR("mysql_read_packet: 读取头失败");
            return -1;
        }
        total += (size_t)n;
    }

    uint32_t pkt_len = read_3byte_length(header);
    uint8_t seq_id = header[3];

    if (pkt_len > max_len) {
        LOG_ERROR("mysql_read_packet: 包长度 %u 超过缓冲区 %zu", pkt_len, max_len);
        return -1;
    }

    /* 读取包体 */
    total = 0;
    while (total < pkt_len) {
#ifdef _WIN32
        ssize_t n = recv(conn->sock, (char *)((uint8_t *)buffer + total), (int)(pkt_len - total), 0);
#else
        ssize_t n = recv(conn->sock, (uint8_t *)buffer + total, pkt_len - total, 0);
#endif
        if (n <= 0) {
            LOG_ERROR("mysql_read_packet: 读取体失败");
            return -1;
        }
        total += (size_t)n;
    }

    return (int)pkt_len;
}

/* ============================================================
 * 协议数据包发送
 * ============================================================ */

/**
 * @brief 发送 MySQL 包（自动添加头部）
 */
static int mysql_send_packet(mysql_connection_t *conn, const void *data, uint32_t len) {
    uint8_t header[4];
    write_3byte_length(header, len);
    header[3] = (uint8_t)mysql_next_packet_id(conn);

    if (mysql_send_raw(conn, header, 4) < 0) return -1;
    if (len > 0 && mysql_send_raw(conn, data, len) < 0) return -1;
    return 0;
}

/* ============================================================
 * 握手实现
 * ============================================================ */

int mysql_handle_handshake(mysql_connection_t *conn) {
    if (!conn) return -1;

    LOG_INFO("mysql_handle_handshake: 开始握手");

    /* 发送 Handshake Packet */
    uint8_t packet[256];
    memset(packet, 0, sizeof(packet));

    int offset = 0;
    /* 协议版本 */
    packet[offset++] = MYSQL_PROTOCOL_VERSION;

    /* 服务器版本字符串 */
    write_cstring(packet + offset, MYSQL_SERVER_VERSION);
    offset += (int)cstring_len(MYSQL_SERVER_VERSION);

    /* 连接 ID */
    write_int4(packet + offset, 1);
    offset += 4;

    /* 认证随机数（前 8 字节） */
    memset(packet + offset, 0x41, 8);
    offset += 8;

    /* 填充字节（1 个 0x00） */
    packet[offset++] = 0x00;

    /* 服务器能力 标志（低位 2 字节） */
    uint16_t capabilities = MYSQL_FLAG_PROTOCOL_41 | MYSQL_FLAG_LONG_FLAG |
                            MYSQL_FLAG_FOUND_ROWS | MYSQL_FLAG_NOT_NULL |
                            MYSQL_FLAG_SECURE_CONNECTION;
    write_int2(packet + offset, capabilities);
    offset += 2;

    /* 字符集 */
    packet[offset++] = 0x21;  /* utf8 (33) */

    /* 服务器状态标志 */
    write_int2(packet + offset, 0x0002);  /* SERVER_STATUS_AUTOCOMMIT */
    offset += 2;

    /* 服务器能力 标志（高位 2 字节） */
    write_int2(packet + offset, 0);
    offset += 2;

    /* 认证随机数长度 */
    packet[offset++] = 0x14;  /* 20 字节 */

    /* 保留 10 字节（0x00） */
    offset += 10;

    /* 认证随机数（剩余 12 字节） */
    memset(packet + offset, 0x42, 12);
    offset += 12;

    if (mysql_send_packet(conn, packet, (uint32_t)offset) < 0) {
        LOG_ERROR("mysql_handle_handshake: 发送握手包失败");
        return -1;
    }

    /* 接收客户端响应（Handshake Response） */
    uint8_t response[1024];
    int resp_len = mysql_read_packet(conn, response, sizeof(response));
    if (resp_len < 0) {
        LOG_ERROR("mysql_handle_handshake: 读取客户端响应失败");
        return -1;
    }

    LOG_INFO("mysql_handle_handshake: 握手完成");
    return 0;
}

/* ============================================================
 * 查询处理
 * ============================================================ */

int mysql_handle_query(mysql_connection_t *conn, const char *query) {
    if (!conn || !query) return -1;

    LOG_INFO("mysql_handle_query: 执行查询 '%s'", query);

    /* TODO: 集成实际的 SQL 执行引擎 */
    /* 这里返回一个示例空结果集 */

    return 0;
}

/* ============================================================
 * 协议数据包发送
 * ============================================================ */

int mysql_send_ok(mysql_connection_t *conn, uint64_t affected_rows,
                  uint64_t last_insert_id) {
    if (!conn) return -1;

    uint8_t packet[256];
    int offset = 0;

    packet[offset++] = 0x00;  /* OK 标志 */

    /* 受影响行数（长度编码） */
    if (affected_rows < 251) {
        packet[offset++] = (uint8_t)affected_rows;
    } else {
        packet[offset++] = 0xFC;
        write_int2(packet + offset, (uint16_t)affected_rows);
        offset += 2;
    }

    /* 最后插入 ID（长度编码） */
    if (last_insert_id < 251) {
        packet[offset++] = (uint8_t)last_insert_id;
    } else {
        packet[offset++] = 0xFC;
        write_int2(packet + offset, (uint16_t)last_insert_id);
        offset += 2;
    }

    /* 服务器状态 */
    write_int2(packet + offset, 0x0002);  /* SERVER_STATUS_AUTOCOMMIT */
    offset += 2;

    /* 警告数 */
    write_int2(packet + offset, 0);
    offset += 2;

    if (mysql_send_packet(conn, packet, (uint32_t)offset) < 0) {
        LOG_ERROR("mysql_send_ok: 发送 OK 包失败");
        return -1;
    }
    return 0;
}

int mysql_send_eof(mysql_connection_t *conn, uint16_t warnings) {
    if (!conn) return -1;

    uint8_t packet[9];
    packet[0] = 0xFE;  /* EOF 标志 */

    /* 警告数 */
    write_int2(packet + 1, warnings);

    /* 服务器状态 */
    write_int2(packet + 3, 0x0002);

    if (mysql_send_packet(conn, packet, 5) < 0) {
        LOG_ERROR("mysql_send_eof: 发送 EOF 包失败");
        return -1;
    }
    return 0;
}

int mysql_send_error(mysql_connection_t *conn, uint16_t code,
                     const char *sqlstate, const char *message) {
    if (!conn) return -1;

    uint8_t packet[1024];
    int offset = 0;

    packet[offset++] = 0xFF;  /* Error 标志 */

    /* 错误码 */
    write_int2(packet + offset, code);
    offset += 2;

    /* '#' + SQLSTATE (5 字符) */
    packet[offset++] = '#';
    if (sqlstate) {
        memcpy(packet + offset, sqlstate, 5);
    } else {
        memcpy(packet + offset, "HY000", 5);
    }
    offset += 5;

    /* 错误消息 */
    if (message) {
        size_t msg_len = strlen(message);
        if (msg_len > 512) msg_len = 512;
        memcpy(packet + offset, message, msg_len);
        offset += (int)msg_len;
    }

    if (mysql_send_packet(conn, packet, (uint32_t)offset) < 0) {
        LOG_ERROR("mysql_send_error: 发送错误包失败");
        return -1;
    }
    return 0;
}

int mysql_send_column_def(mysql_connection_t *conn,
                          const mysql_column_def_t *column) {
    if (!conn || !column) return -1;

    uint8_t packet[1024];
    int offset = 0;

    /* 目录名 */
    packet[offset++] = 0x03;  /* 长度 = 3 */
    memcpy(packet + offset, "def", 3);
    offset += 3;

    /* 库名 */
    size_t len = column->schema ? strlen(column->schema) : 0;
    if (len < 251) {
        packet[offset++] = (uint8_t)len;
    } else {
        packet[offset++] = 0xFC;
        write_int2(packet + offset, (uint16_t)len);
        offset += 2;
    }
    if (len > 0) {
        memcpy(packet + offset, column->schema, len);
        offset += (int)len;
    }

    /* 表名 */
    len = column->table ? strlen(column->table) : 0;
    if (len < 251) {
        packet[offset++] = (uint8_t)len;
    } else {
        packet[offset++] = 0xFC;
        write_int2(packet + offset, (uint16_t)len);
        offset += 2;
    }
    if (len > 0) {
        memcpy(packet + offset, column->table, len);
        offset += (int)len;
    }

    /* 原始表名 */
    len = column->org_table ? strlen(column->org_table) : 0;
    if (len < 251) {
        packet[offset++] = (uint8_t)len;
    } else {
        packet[offset++] = 0xFC;
        write_int2(packet + offset, (uint16_t)len);
        offset += 2;
    }
    if (len > 0) {
        memcpy(packet + offset, column->org_table, len);
        offset += (int)len;
    }

    /* 列名 */
    len = column->name ? strlen(column->name) : 0;
    if (len < 251) {
        packet[offset++] = (uint8_t)len;
    } else {
        packet[offset++] = 0xFC;
        write_int2(packet + offset, (uint16_t)len);
        offset += 2;
    }
    if (len > 0) {
        memcpy(packet + offset, column->name, len);
        offset += (int)len;
    }

    /* 原始列名 */
    len = column->org_name ? strlen(column->org_name) : 0;
    if (len < 251) {
        packet[offset++] = (uint8_t)len;
    } else {
        packet[offset++] = 0xFC;
        write_int2(packet + offset, (uint16_t)len);
        offset += 2;
    }
    if (len > 0) {
        memcpy(packet + offset, column->org_name, len);
        offset += (int)len;
    }

    /* 填充 0x0C */
    packet[offset++] = 0x0C;

    /* 字符集编号 */
    write_int2(packet + offset, column->charsetnr);
    offset += 2;

    /* 列长度 */
    write_int4(packet + offset, column->length);
    offset += 4;

    /* 列类型 */
    packet[offset++] = column->type;

    /* 列标志 */
    write_int2(packet + offset, column->flags);
    offset += 2;

    /* 小数位数 */
    packet[offset++] = column->decimals;

    /* 填充 0x00 0x00 */
    packet[offset++] = 0x00;
    packet[offset++] = 0x00;

    if (mysql_send_packet(conn, packet, (uint32_t)offset) < 0) {
        LOG_ERROR("mysql_send_column_def: 发送列定义失败");
        return -1;
    }
    return 0;
}

int mysql_send_row(mysql_connection_t *conn, const char * const *fields,
                   const uint32_t *lengths, int num_fields) {
    if (!conn) return -1;

    uint8_t packet[65536];
    int offset = 0;

    for (int i = 0; i < num_fields; i++) {
        uint32_t len = lengths ? lengths[i] : (fields[i] ? (uint32_t)strlen(fields[i]) : 0);

        if (len < 251) {
            packet[offset++] = (uint8_t)len;
        } else if (len < 65536) {
            packet[offset++] = 0xFC;
            write_int2(packet + offset, (uint16_t)len);
            offset += 2;
        } else {
            packet[offset++] = 0xFD;
            write_int4(packet + offset, len);
            offset += 4;
        }

        if (len > 0 && fields[i]) {
            memcpy(packet + offset, fields[i], len);
            offset += (int)len;
        }
    }

    if (mysql_send_packet(conn, packet, (uint32_t)offset) < 0) {
        LOG_ERROR("mysql_send_row: 发送行数据失败");
        return -1;
    }
    return 0;
}

int mysql_send_row_nullable(mysql_connection_t *conn,
                            const char * const *fields, int num_fields) {
    if (!conn) return -1;

    uint8_t packet[65536];
    int offset = 0;

    for (int i = 0; i < num_fields; i++) {
        if (!fields[i]) {
            packet[offset++] = 0xFB;  /* NULL 标记 */
            continue;
        }

        uint32_t len = (uint32_t)strlen(fields[i]);
        if (len < 251) {
            packet[offset++] = (uint8_t)len;
        } else if (len < 65536) {
            packet[offset++] = 0xFC;
            write_int2(packet + offset, (uint16_t)len);
            offset += 2;
        } else {
            packet[offset++] = 0xFD;
            write_int4(packet + offset, len);
            offset += 4;
        }

        memcpy(packet + offset, fields[i], len);
        offset += (int)len;
    }

    if (mysql_send_packet(conn, packet, (uint32_t)offset) < 0) {
        LOG_ERROR("mysql_send_row_nullable: 发送行数据失败");
        return -1;
    }
    return 0;
}

int mysql_send_result(mysql_connection_t *conn, mysql_result_t *result) {
    if (!conn || !result) return -1;

    /* 检查是否有错误 */
    if (result->error_code != 0) {
        return mysql_send_error(conn, (uint16_t)result->error_code,
                                "HY000", result->error_msg);
    }

    /* 发送列数 */
    uint8_t packet[4];
    packet[0] = 0x00;
    if (result->num_columns < 251) {
        packet[1] = (uint8_t)result->num_columns;
        mysql_send_packet(conn, packet, 2);
    } else {
        packet[1] = 0xFC;
        write_int2(packet + 2, (uint16_t)result->num_columns);
        mysql_send_packet(conn, packet, 4);
    }

    /* 发送列定义 */
    for (int i = 0; i < result->num_columns; i++) {
        if (mysql_send_column_def(conn, &result->columns[i]) < 0) {
            return -1;
        }
    }

    /* 发送 EOF（列定义结束） */
    if (mysql_send_eof(conn, 0) < 0) {
        return -1;
    }

    /* 发送行数据 */
    for (int i = 0; i < result->num_rows; i++) {
        if (result->rows[i]) {
            /* 简单处理：每行假设为单个字段 */
            uint32_t len = (uint32_t)strlen(result->rows[i]);
            uint8_t row_packet[65536];
            int offset = 0;

            if (len < 251) {
                row_packet[offset++] = (uint8_t)len;
            } else {
                row_packet[offset++] = 0xFC;
                write_int2(row_packet + offset, (uint16_t)len);
                offset += 2;
            }

            memcpy(row_packet + offset, result->rows[i], len);
            offset += (int)len;

            if (mysql_send_packet(conn, row_packet, (uint32_t)offset) < 0) {
                return -1;
            }
        }
    }

    /* 发送 EOF（数据结束） */
    if (mysql_send_eof(conn, 0) < 0) {
        return -1;
    }

    return 0;
}

/* ============================================================
 * 结果集管理
 * ============================================================ */

void mysql_result_init(mysql_result_t *result, int num_columns) {
    if (!result) return;
    memset(result, 0, sizeof(mysql_result_t));
    result->num_columns = num_columns;
    if (num_columns > 0) {
        result->columns = (mysql_column_def_t *)calloc((size_t)num_columns, sizeof(mysql_column_def_t));
    }
}

void mysql_result_free(mysql_result_t *result) {
    if (!result) return;
    if (result->columns) {
        for (int i = 0; i < result->num_columns; i++) {
            mysql_column_def_t *col = &result->columns[i];
            if (col->catalog) free(col->catalog);
            if (col->schema) free(col->schema);
            if (col->table) free(col->table);
            if (col->org_table) free(col->org_table);
            if (col->name) free(col->name);
            if (col->org_name) free(col->org_name);
        }
        free(result->columns);
    }
    if (result->rows) {
        for (int i = 0; i < result->num_rows; i++) {
            if (result->rows[i]) free(result->rows[i]);
        }
        free(result->rows);
    }
    if (result->error_msg) free(result->error_msg);
    memset(result, 0, sizeof(mysql_result_t));
}
