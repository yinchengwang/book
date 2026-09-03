/**
 * @file copy_binary.c
 * @brief 二进制格式 COPY 实现
 *
 * 二进制格式使用固定字节序（大端序），
 * 参考 PostgreSQL 的 binary format。
 */

#include "db/tools/copy.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef _WIN32
/* Windows 下使用内联字节序转换函数，避免 Win32 API 冲突 */
static uint32_t htonl(uint32_t hostlong)
{
    return ((hostlong & 0xFF000000) >> 24) |
           ((hostlong & 0x00FF0000) >> 8)  |
           ((hostlong & 0x0000FF00) << 8)  |
           ((hostlong & 0x000000FF) << 24);
}

static uint32_t ntohl(uint32_t netlong)
{
    return htonl(netlong);  /* 对称操作 */
}
#else
#include <arpa/inet.h>
#endif

/* ─────────────────────────────────────────────────────────────────
 * 二进制格式常量
 * ───────────────────────────────────────────────────────────────── */

#define BINARY_MAGIC    0x5047434F  /* "PGCO" */
#define BINARY_VERSION  0x0001

/* ─────────────────────────────────────────────────────────────────
 * 二进制头文件结构
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 二进制文件头
 */
typedef struct {
    uint32_t magic;      /* 魔数 BINARY_MAGIC */
    uint32_t version;    /* 格式版本 */
    uint32_t flags;      /* 标志位 */
    uint32_t n_fields;   /* 字段数 */
} BinaryHeader;

/* ─────────────────────────────────────────────────────────────────
 * 二进制导出
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 写入二进制头文件
 */
int binary_write_header(FILE *fp, int num_columns)
{
    if (!fp) return -1;

    BinaryHeader header;
    header.magic = htonl(BINARY_MAGIC);
    header.version = htonl(BINARY_VERSION);
    header.flags = 0;
    header.n_fields = htonl((uint32_t)num_columns);

    fwrite(&header, sizeof(header), 1, fp);
    return 0;
}

/**
 * @brief 写入二进制行
 * @param fp 输出文件
 * @param values 值数组
 * @param num_columns 列数
 * @return 0 成功，非 0 失败
 */
int binary_write_row(FILE *fp, const char **values, int num_columns)
{
    if (!fp || !values) return -1;

    /* 每个字段：4字节长度 + 数据 */
    for (int i = 0; i < num_columns; i++) {
        uint32_t len;

        if (values[i] == NULL) {
            len = 0xFFFFFFFF;  /* NULL 标记 */
            len = htonl(len);
            fwrite(&len, sizeof(len), 1, fp);
        } else {
            size_t field_len = strlen(values[i]);
            len = htonl((uint32_t)field_len);
            fwrite(&len, sizeof(len), 1, fp);
            fwrite(values[i], 1, field_len, fp);
        }
    }

    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * 二进制导入
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 读取二进制头文件
 */
int binary_read_header(FILE *fp, int *num_columns)
{
    if (!fp || !num_columns) return -1;

    BinaryHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        return -1;
    }

    if (ntohl(header.magic) != BINARY_MAGIC) {
        return -1;  /* 无效魔数 */
    }

    *num_columns = (int)ntohl(header.n_fields);
    return 0;
}

/**
 * @brief 读取二进制字段
 */
int binary_read_field(FILE *fp, char **value, uint32_t *len)
{
    if (!fp || !value || !len) return -1;

    uint32_t field_len;
    if (fread(&field_len, sizeof(field_len), 1, fp) != 1) {
        return -1;
    }

    field_len = ntohl(field_len);

    if (field_len == 0xFFFFFFFF) {
        /* NULL 字段 */
        *value = NULL;
        *len = 0;
        return 0;
    }

    *len = field_len;
    *value = (char *)malloc(field_len + 1);
    if (!*value) return -1;

    if (fread(*value, 1, field_len, fp) != field_len) {
        free(*value);
        *value = NULL;
        return -1;
    }

    (*value)[field_len] = '\0';
    return 0;
}