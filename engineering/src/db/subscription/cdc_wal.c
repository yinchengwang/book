/**
 * @file cdc_wal.c
 * @brief CDC WAL 解析器实现
 *
 * 解析 WAL 日志文件，提取变更记录供 CDC 使用。
 */
#include "db/subscription/cdc.h"
#include "db/core/log.h"
#include "db/wal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* WAL 段大小（默认值，如果 wal.h 未定义） */
#ifndef WAL_SEGMENT_SIZE
#define WAL_SEGMENT_SIZE (16 * 1024 * 1024)  /* 16MB */
#endif

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * WAL 解析器内部结构
 * ======================================================================== */

/**
 * @brief WAL 解析器
 */
typedef struct wal_parser_s {
    char wal_dir[512];           /**< WAL 目录 */
    FILE *current_file;          /**< 当前文件 */
    char current_filename[256];  /**< 当前文件名 */
    uint64_t current_lsn;        /**< 当前解析位置 */
    uint64_t last_checkpoint_lsn; /**< 最后检查点 LSN */
    void *buffer;                /**< 解析缓冲区 */
    size_t buffer_size;          /**< 缓冲区大小 */
} wal_parser_t;

/* ========================================================================
 * 内部函数
 * ======================================================================== */

/**
 * @brief 获取下一个 WAL 文件名
 */
static int get_next_wal_file(wal_parser_t *parser, char *filename, size_t size) {
    if (parser == NULL || filename == NULL) return -1;

    /* 简化实现：扫描目录找最新的 WAL 文件 */
    /* TODO: 实现完整的 WAL 段文件管理 */

    snprintf(filename, size, "%s/0000000000000000.wal", parser->wal_dir);
    return 0;
}

/**
 * @brief 解析日志记录头
 */
static int parse_record_header(const uint8_t *data, size_t len,
                               wal_record_header_t *out_header) {
    if (data == NULL || out_header == NULL || len < WAL_RECORD_HEADER_SIZE) {
        return -1;
    }

    memcpy(out_header, data, WAL_RECORD_HEADER_SIZE);

    /* 解析 3 字节的大小字段 */
    out_header->size[0] = data[offsetof(wal_record_header_t, size) + 0];
    out_header->size[1] = data[offsetof(wal_record_header_t, size) + 1];
    out_header->size[2] = data[offsetof(wal_record_header_t, size) + 2];

    return 0;
}

/**
 * @brief 从 WAL 记录提取 CDC 变更
 */
static cdc_change_t *extract_cdc_change(const wal_record_header_t *wal_header,
                                        const uint8_t *record_data) {
    if (wal_header == NULL) return NULL;

    cdc_change_t *change = (cdc_change_t *)calloc(1, sizeof(cdc_change_t));
    if (change == NULL) return NULL;

    change->lsn = wal_header->lsn;
    change->transaction_id = wal_header->txn_id;
    change->timestamp = 0;  /* TODO: 从 WAL 时间戳获取 */

    /* 根据 WAL 类型设置 CDC 变更类型 */
    switch (wal_header->type) {
        case WAL_LOG_INSERT:
            change->type = CDC_CHANGE_INSERT;
            break;
        case WAL_LOG_UPDATE:
            change->type = CDC_CHANGE_UPDATE;
            break;
        case WAL_LOG_DELETE:
            change->type = CDC_CHANGE_DELETE;
            break;
        default:
            /* 忽略其他类型 */
            free(change);
            return NULL;
    }

    /* 解析记录数据获取关系 ID 和元组 */
    /* WAL 记录数据格式: [relation_id (4 bytes)][tuple_data] */
    if (record_data != NULL && wal_header->size[0] > 0) {
        /* 获取关系 ID */
        change->relation_id = *(int32_t *)record_data;

        /* 元组数据从第 5 字节开始 */
        const uint8_t *tuple_data = record_data + 4;
        size_t tuple_size = wal_header->size[0] - 4;

        if (tuple_size > 0 && change->type == CDC_CHANGE_INSERT) {
            change->new_tuple_size = (int32_t)tuple_size;
            change->new_tuple = malloc(tuple_size);
            if (change->new_tuple != NULL) {
                memcpy(change->new_tuple, tuple_data, tuple_size);
            }
        } else if (tuple_size > 0 && change->type == CDC_CHANGE_DELETE) {
            change->old_tuple_size = (int32_t)tuple_size;
            change->old_tuple = malloc(tuple_size);
            if (change->old_tuple != NULL) {
                memcpy(change->old_tuple, tuple_data, tuple_size);
            }
        }
    }

    return change;
}

/* ========================================================================
 * WAL 解析器 API 实现
 * ======================================================================== */

/**
 * @brief 创建 WAL 解析器
 */
wal_parser_t *wal_parser_create(const char *wal_dir) {
    if (wal_dir == NULL) {
        LOG_ERROR("WAL 解析器创建失败: wal_dir 为空");
        return NULL;
    }

    wal_parser_t *parser = (wal_parser_t *)calloc(1, sizeof(wal_parser_t));
    if (parser == NULL) {
        LOG_ERROR("WAL 解析器分配失败");
        return NULL;
    }

    strncpy(parser->wal_dir, wal_dir, sizeof(parser->wal_dir) - 1);
    parser->current_file = NULL;
    parser->current_lsn = 0;
    parser->last_checkpoint_lsn = 0;
    parser->buffer_size = WAL_BUFFER_SIZE;
    parser->buffer = malloc(parser->buffer_size);

    if (parser->buffer == NULL) {
        free(parser);
        return NULL;
    }

    LOG_INFO("WAL 解析器创建成功: dir=%s", wal_dir);
    return parser;
}

/**
 * @brief 销毁 WAL 解析器
 */
void wal_parser_destroy(wal_parser_t *parser) {
    if (parser == NULL) return;

    if (parser->current_file != NULL) {
        fclose(parser->current_file);
    }
    free(parser->buffer);
    free(parser);

    LOG_INFO("WAL 解析器已销毁");
}

/**
 * @brief 打开 WAL 文件
 */
int wal_parser_open(wal_parser_t *parser, uint64_t start_lsn) {
    if (parser == NULL) return -1;

    /* 关闭已打开的文件 */
    if (parser->current_file != NULL) {
        fclose(parser->current_file);
        parser->current_file = NULL;
    }

    /* 构建文件名 */
    char filename[512];
    /* WAL 文件名格式: 段号.lsn.wal */
    uint64_t segment = start_lsn / WAL_SEGMENT_SIZE;
    snprintf(filename, sizeof(filename), "%s/%016llx.wal",
             parser->wal_dir, (unsigned long long)segment);

    parser->current_file = fopen(filename, "rb");
    if (parser->current_file == NULL) {
        LOG_INFO("WAL 文件不存在: %s", filename);
        return -1;
    }

    /* 跳过文件头 */
    fseek(parser->current_file, WAL_HEADER_SIZE, SEEK_SET);
    parser->current_lsn = start_lsn;

    LOG_INFO("WAL 文件已打开: %s, start_lsn=%lu", filename, start_lsn);
    return 0;
}

/**
 * @brief 关闭 WAL 文件
 */
void wal_parser_close(wal_parser_t *parser) {
    if (parser == NULL) return;

    if (parser->current_file != NULL) {
        fclose(parser->current_file);
        parser->current_file = NULL;
    }
}

/**
 * @brief 获取下一条变更记录
 */
cdc_change_t *wal_parser_next_change(wal_parser_t *parser) {
    if (parser == NULL || parser->current_file == NULL) return NULL;

    /* 读取记录头 */
    uint8_t header[WAL_RECORD_HEADER_SIZE];
    size_t read = fread(header, 1, WAL_RECORD_HEADER_SIZE, parser->current_file);
    if (read < WAL_RECORD_HEADER_SIZE) {
        return NULL;  /* 文件结束或读取错误 */
    }

    /* 解析记录头 */
    wal_record_header_t wal_header;
    wal_header.type = header[0];
    wal_header.size[0] = header[1];
    wal_header.size[1] = header[2];
    wal_header.size[2] = header[3];
    wal_header.lsn = *(uint64_t *)(header + 4);
    wal_header.txn_id = *(uint32_t *)(header + 12);
    wal_header.prev_lsn = *(uint32_t *)(header + 16);

    /* 计算记录大小 */
    size_t record_size = wal_header.size[0] | (wal_header.size[1] << 8) |
                         (wal_header.size[2] << 16);

    /* 检查记录大小 */
    if (record_size == 0 || record_size > parser->buffer_size) {
        return NULL;
    }

    /* 读取记录数据 */
    uint8_t *record_data = (uint8_t *)parser->buffer;
    read = fread(record_data, 1, record_size, parser->current_file);
    if (read < record_size) {
        return NULL;
    }

    /* 更新 LSN */
    parser->current_lsn = wal_header.lsn;

    /* 提取 CDC 变更 */
    return extract_cdc_change(&wal_header, record_data);
}

/**
 * @brief 设置解析起始位置
 */
void wal_parser_set_position(wal_parser_t *parser, uint64_t lsn) {
    if (parser == NULL) return;
    parser->current_lsn = lsn;
}

/**
 * @brief 获取当前解析位置
 */
uint64_t wal_parser_get_position(const wal_parser_t *parser) {
    if (parser == NULL) return 0;
    return parser->current_lsn;
}

/* ========================================================================
 * CDC WAL 集成 API 实现
 * ======================================================================== */

/**
 * @brief 从 WAL 解析变更
 *
 * @param ctx CDC 上下文
 * @param wal_dir WAL 目录
 * @param start_lsn 起始 LSN
 * @param out_changes 输出变更数组
 * @param max_changes 最大变更数
 * @return 实际解析的变更数
 */
int32_t cdc_parse_wal(cdc_context_t *ctx,
                      const char *wal_dir,
                      uint64_t start_lsn,
                      cdc_change_t **out_changes,
                      int32_t max_changes) {
    if (ctx == NULL || wal_dir == NULL || out_changes == NULL || max_changes <= 0) {
        return -1;
    }

    wal_parser_t *parser = wal_parser_create(wal_dir);
    if (parser == NULL) return -1;

    if (wal_parser_open(parser, start_lsn) != 0) {
        wal_parser_destroy(parser);
        return 0;  /* 无 WAL 文件 */
    }

    int32_t count = 0;
    cdc_change_t *change;

    while (count < max_changes && (change = wal_parser_next_change(parser)) != NULL) {
        /* 检查是否监控该关系 */
        if (cdc_is_relation_monitored(ctx, change->relation_id)) {
            out_changes[count++] = change;
        } else {
            cdc_free_change(change);
        }
    }

    /* 更新 CDC LSN */
    cdc_set_lsn(ctx, wal_parser_get_position(parser));

    wal_parser_destroy(parser);

    LOG_INFO("CDC WAL 解析完成: start_lsn=%lu, changes=%d", start_lsn, count);
    return count;
}

/**
 * @brief 提取单条变更记录
 *
 * @param wal_dir WAL 目录
 * @param lsn 要提取的 LSN
 * @return 变更记录，NULL 表示未找到
 */
cdc_change_t *cdc_extract_change(const char *wal_dir, uint64_t lsn) {
    if (wal_dir == NULL) return NULL;

    wal_parser_t *parser = wal_parser_create(wal_dir);
    if (parser == NULL) return NULL;

    /* 计算段号 */
    uint64_t segment = lsn / WAL_SEGMENT_SIZE;

    /* 打开对应段文件 */
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/%016llx.wal",
             wal_dir, (unsigned long long)segment);

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        wal_parser_destroy(parser);
        return NULL;
    }

    /* 跳过文件头 */
    fseek(fp, WAL_HEADER_SIZE, SEEK_SET);

    cdc_change_t *result = NULL;
    uint8_t header[WAL_RECORD_HEADER_SIZE];

    /* 扫描文件查找目标 LSN */
    while (fread(header, 1, WAL_RECORD_HEADER_SIZE, fp) == WAL_RECORD_HEADER_SIZE) {
        wal_record_header_t wal_header;
        wal_header.type = header[0];
        wal_header.size[0] = header[1];
        wal_header.size[1] = header[2];
        wal_header.size[2] = header[3];
        wal_header.lsn = *(uint64_t *)(header + 4);
        wal_header.txn_id = *(uint32_t *)(header + 12);

        /* 检查 LSN */
        if (wal_header.lsn == lsn) {
            size_t record_size = wal_header.size[0] | (wal_header.size[1] << 8) |
                                 (wal_header.size[2] << 16);

            if (record_size > 0 && record_size < 1024 * 1024) {
                uint8_t *data = (uint8_t *)malloc(record_size);
                if (data != NULL && fread(data, 1, record_size, fp) == record_size) {
                    result = extract_cdc_change(&wal_header, data);
                }
                free(data);
            }
            break;
        }

        /* 跳过当前记录数据 */
        size_t record_size = wal_header.size[0] | (wal_header.size[1] << 8) |
                             (wal_header.size[2] << 16);
        fseek(fp, record_size, SEEK_CUR);
    }

    fclose(fp);
    wal_parser_destroy(parser);

    return result;
}
