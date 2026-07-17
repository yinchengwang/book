/**
 * @file cdc.c
 * @brief 变更数据捕获实现
 *
 * 实现 CDC 核心功能，捕获数据库的变更操作。
 */
#include "db/subscription/cdc.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/**
 * @brief CDC 变更缓冲区
 */
typedef struct cdc_change_buffer_s {
    cdc_change_t *head;     /**< 缓冲区头部 */
    cdc_change_t *tail;     /**< 缓冲区尾部 */
    int32_t count;          /**< 缓冲区数量 */
} cdc_change_buffer_t;

/* ========================================================================
 * 内部函数
 * ======================================================================== */

/**
 * @brief 分配变更记录
 */
static cdc_change_t *alloc_change(cdc_context_t *ctx,
                                  cdc_change_type_t type,
                                  int32_t relation_id,
                                  uint64_t transaction_id) {
    if (ctx == NULL) return NULL;

    cdc_change_t *change = (cdc_change_t *)calloc(1, sizeof(cdc_change_t));
    if (change == NULL) return NULL;

    change->type = type;
    change->relation_id = relation_id;
    change->transaction_id = transaction_id;
    change->lsn = ++ctx->current_lsn;
    change->timestamp = (uint64_t)time(NULL) * 1000000;  /* 微秒 */
    change->next = NULL;

    return change;
}

/**
 * @brief 释放变更记录
 */
static void free_change(cdc_change_t *change) {
    if (change == NULL) return;

    free(change->old_tuple);
    free(change->new_tuple);
    free(change);
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

cdc_context_t *cdc_create(const char *data_dir, uint64_t start_lsn) {
    if (data_dir == NULL) {
        LOG_ERROR("CDC 创建失败: data_dir 为空");
        return NULL;
    }

    cdc_context_t *ctx = (cdc_context_t *)calloc(1, sizeof(cdc_context_t));
    if (ctx == NULL) {
        LOG_ERROR("CDC 上下文分配失败");
        return NULL;
    }

    strncpy(ctx->data_dir, data_dir, sizeof(ctx->data_dir) - 1);
    ctx->start_lsn = start_lsn;
    ctx->current_lsn = start_lsn;
    ctx->relation_count = 0;
    ctx->relation_ids = NULL;

    /* 分配初始关系数组 */
    ctx->relation_ids = (int32_t *)malloc(CDC_MAX_RELATIONS * sizeof(int32_t));
    if (ctx->relation_ids == NULL) {
        free(ctx);
        return NULL;
    }

    /* 创建数据目录 */
#ifdef _WIN32
    if (mkdir(data_dir) != 0 && errno != EEXIST) {
#else
    if (mkdir(data_dir, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_WARN("CDC 数据目录创建失败: %s", data_dir);
    }

    LOG_INFO("CDC 上下文创建成功: start_lsn=%lu", start_lsn);
    return ctx;
}

void cdc_destroy(cdc_context_t *ctx) {
    if (ctx == NULL) return;

    /* 保存状态 */
    cdc_save_state(ctx);

    free(ctx->relation_ids);
    free(ctx);
    LOG_INFO("CDC 上下文已销毁");
}

void cdc_reset(cdc_context_t *ctx) {
    if (ctx == NULL) return;

    ctx->current_lsn = ctx->start_lsn;
    LOG_INFO("CDC 状态已重置");
}

/* ========================================================================
 * 配置 API 实现
 * ======================================================================== */

int cdc_add_relation(cdc_context_t *ctx, int32_t relation_id) {
    if (ctx == NULL) return -1;

    /* 检查是否已存在 */
    for (int32_t i = 0; i < ctx->relation_count; i++) {
        if (ctx->relation_ids[i] == relation_id) {
            return 0;  /* 已存在 */
        }
    }

    /* 检查容量 */
    if (ctx->relation_count >= CDC_MAX_RELATIONS) {
        LOG_ERROR("CDC 关系数达到上限");
        return -1;
    }

    ctx->relation_ids[ctx->relation_count++] = relation_id;
    LOG_INFO("CDC 添加监控关系: relation_id=%d", relation_id);
    return 0;
}

int cdc_remove_relation(cdc_context_t *ctx, int32_t relation_id) {
    if (ctx == NULL) return -1;

    for (int32_t i = 0; i < ctx->relation_count; i++) {
        if (ctx->relation_ids[i] == relation_id) {
            /* 移动数组元素 */
            for (int32_t j = i; j < ctx->relation_count - 1; j++) {
                ctx->relation_ids[j] = ctx->relation_ids[j + 1];
            }
            ctx->relation_count--;
            LOG_INFO("CDC 移除监控关系: relation_id=%d", relation_id);
            return 0;
        }
    }

    return -1;  /* 未找到 */
}

bool cdc_is_relation_monitored(const cdc_context_t *ctx, int32_t relation_id) {
    if (ctx == NULL) return false;

    for (int32_t i = 0; i < ctx->relation_count; i++) {
        if (ctx->relation_ids[i] == relation_id) {
            return true;
        }
    }

    return false;
}

/* ========================================================================
 * 变更捕获 API 实现
 * ======================================================================== */

int cdc_record_insert(cdc_context_t *ctx,
                      int32_t relation_id,
                      uint64_t transaction_id,
                      const void *tuple,
                      int32_t tuple_size) {
    if (ctx == NULL || tuple == NULL || tuple_size <= 0) return -1;

    /* 检查是否监控该关系 */
    if (!cdc_is_relation_monitored(ctx, relation_id)) {
        return 0;  /* 不监控，静默忽略 */
    }

    cdc_change_t *change = alloc_change(ctx, CDC_CHANGE_INSERT, relation_id, transaction_id);
    if (change == NULL) return -1;

    change->new_tuple_size = tuple_size;
    change->new_tuple = malloc((size_t)tuple_size);
    if (change->new_tuple == NULL) {
        free(change);
        return -1;
    }
    memcpy(change->new_tuple, tuple, (size_t)tuple_size);

    LOG_DEBUG("CDC 记录插入: relation_id=%d, lsn=%lu", relation_id, change->lsn);
    return 0;
}

int cdc_record_update(cdc_context_t *ctx,
                      int32_t relation_id,
                      uint64_t transaction_id,
                      const void *old_tuple,
                      int32_t old_tuple_size,
                      const void *new_tuple,
                      int32_t new_tuple_size) {
    if (ctx == NULL) return -1;

    /* 检查是否监控该关系 */
    if (!cdc_is_relation_monitored(ctx, relation_id)) {
        return 0;
    }

    cdc_change_t *change = alloc_change(ctx, CDC_CHANGE_UPDATE, relation_id, transaction_id);
    if (change == NULL) return -1;

    if (old_tuple != NULL && old_tuple_size > 0) {
        change->old_tuple_size = old_tuple_size;
        change->old_tuple = malloc((size_t)old_tuple_size);
        if (change->old_tuple != NULL) {
            memcpy(change->old_tuple, old_tuple, (size_t)old_tuple_size);
        }
    }

    if (new_tuple != NULL && new_tuple_size > 0) {
        change->new_tuple_size = new_tuple_size;
        change->new_tuple = malloc((size_t)new_tuple_size);
        if (change->new_tuple != NULL) {
            memcpy(change->new_tuple, new_tuple, (size_t)new_tuple_size);
        }
    }

    LOG_DEBUG("CDC 记录更新: relation_id=%d, lsn=%lu", relation_id, change->lsn);
    return 0;
}

int cdc_record_delete(cdc_context_t *ctx,
                      int32_t relation_id,
                      uint64_t transaction_id,
                      const void *tuple,
                      int32_t tuple_size) {
    if (ctx == NULL || tuple == NULL || tuple_size <= 0) return -1;

    /* 检查是否监控该关系 */
    if (!cdc_is_relation_monitored(ctx, relation_id)) {
        return 0;
    }

    cdc_change_t *change = alloc_change(ctx, CDC_CHANGE_DELETE, relation_id, transaction_id);
    if (change == NULL) return -1;

    change->old_tuple_size = tuple_size;
    change->old_tuple = malloc((size_t)tuple_size);
    if (change->old_tuple == NULL) {
        free(change);
        return -1;
    }
    memcpy(change->old_tuple, tuple, (size_t)tuple_size);

    LOG_DEBUG("CDC 记录删除: relation_id=%d, lsn=%lu", relation_id, change->lsn);
    return 0;
}

/* ========================================================================
 * 变更获取 API 实现
 * ======================================================================== */

cdc_change_t *cdc_get_next_change(cdc_context_t *ctx) {
    /* TODO: 实现变更队列获取 */
    (void)ctx;
    return NULL;
}

void cdc_free_change(cdc_change_t *change) {
    free_change(change);
}

int32_t cdc_get_pending_count(const cdc_context_t *ctx) {
    if (ctx == NULL) return 0;
    /* TODO: 实现待处理计数 */
    return 0;
}

/* ========================================================================
 * 持久化 API 实现
 * ======================================================================== */

static int get_state_file_path(const cdc_context_t *ctx, char *path, size_t size) {
    snprintf(path, size, "%s/cdc.state", ctx->data_dir);
    return 0;
}

int cdc_save_state(const cdc_context_t *ctx) {
    if (ctx == NULL) return -1;

    char path[512];
    get_state_file_path(ctx, path, sizeof(path));

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        LOG_ERROR("CDC 状态文件创建失败: %s", path);
        return -1;
    }

    /* 写入魔数 */
    uint32_t magic = CDC_FILE_MAGIC;
    fwrite(&magic, sizeof(magic), 1, fp);

    /* 写入版本 */
    uint32_t version = CDC_FILE_VERSION;
    fwrite(&version, sizeof(version), 1, fp);

    /* 写入状态 */
    fwrite(&ctx->start_lsn, sizeof(ctx->start_lsn), 1, fp);
    fwrite(&ctx->current_lsn, sizeof(ctx->current_lsn), 1, fp);
    fwrite(&ctx->relation_count, sizeof(ctx->relation_count), 1, fp);
    fwrite(ctx->relation_ids, sizeof(int32_t), ctx->relation_count, fp);

    fclose(fp);
    LOG_INFO("CDC 状态已保存: lsn=%lu", ctx->current_lsn);
    return 0;
}

int cdc_load_state(cdc_context_t *ctx) {
    if (ctx == NULL) return -1;

    char path[512];
    get_state_file_path(ctx, path, sizeof(path));

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        LOG_INFO("CDC 状态文件不存在，将从头开始");
        return 0;
    }

    /* 读取并验证魔数 */
    uint32_t magic;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != CDC_FILE_MAGIC) {
        LOG_WARN("CDC 状态文件魔数无效");
        fclose(fp);
        return -1;
    }

    /* 读取版本 */
    uint32_t version;
    if (fread(&version, sizeof(version), 1, fp) != 1 || version != CDC_FILE_VERSION) {
        LOG_WARN("CDC 状态文件版本不匹配");
        fclose(fp);
        return -1;
    }

    /* 读取状态 */
    fread(&ctx->start_lsn, sizeof(ctx->start_lsn), 1, fp);
    fread(&ctx->current_lsn, sizeof(ctx->current_lsn), 1, fp);
    fread(&ctx->relation_count, sizeof(ctx->relation_count), 1, fp);
    fread(ctx->relation_ids, sizeof(int32_t), ctx->relation_count, fp);

    fclose(fp);
    LOG_INFO("CDC 状态已加载: lsn=%lu", ctx->current_lsn);
    return 0;
}

uint64_t cdc_get_current_lsn(const cdc_context_t *ctx) {
    if (ctx == NULL) return 0;
    return ctx->current_lsn;
}

void cdc_set_lsn(cdc_context_t *ctx, uint64_t lsn) {
    if (ctx == NULL) return;
    ctx->current_lsn = lsn;
    if (ctx->start_lsn == 0) {
        ctx->start_lsn = lsn;
    }
}
