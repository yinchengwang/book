/**
 * @file vector_wal.c
 * @brief 向量 WAL (Write-Ahead Logging) 实现
 *
 * 支持向量操作的预写日志，保证崩溃恢复能力。
 */
#define _POSIX_C_SOURCE 200809L

#include <db/storage/vector/vector_persist.h>
#include <db/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 生成 WAL 文件路径
 */
static void wal_make_path(char *buf, size_t buf_size,
                         const char *path, uint32_t segment_id) {
    snprintf(buf, buf_size, "%s/vector_wal_%u.log", path, segment_id);
}

/**
 * @brief 计算 CRC32 校验和
 */
static uint32_t wal_crc32(const void *data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < size; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

/* ============================================================
 * WAL 创建/打开/关闭
 * ============================================================ */

/**
 * @brief 创建 WAL
 */
vector_wal_t *vector_wal_create(const char *path, uint32_t segment_id) {
    if (!path) {
        LOG_ERROR("WAL 路径为空");
        return NULL;
    }

    vector_wal_t *wal = (vector_wal_t *)calloc(1, sizeof(vector_wal_t));
    if (!wal) {
        LOG_ERROR("分配 WAL 失败");
        return NULL;
    }

    /* 生成文件路径 */
    wal_make_path(wal->path, sizeof(wal->path), path, segment_id);

    /* 打开文件 */
    wal->fp = fopen(wal->path, "wb");
    if (!wal->fp) {
        LOG_ERROR("创建 WAL 文件失败: %s", wal->path);
        free(wal);
        return NULL;
    }

    /* 分配缓冲区 */
    wal->buffer = (uint8_t *)malloc(VECTOR_WAL_BUFFER_SIZE);
    if (!wal->buffer) {
        LOG_ERROR("分配 WAL 缓冲区失败");
        fclose(wal->fp);
        free(wal);
        return NULL;
    }

    /* 初始化头部 */
    vector_wal_file_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = VECTOR_WAL_MAGIC;
    header.version = VECTOR_WAL_VERSION;
    header.created_at = (uint64_t)time(NULL);
    header.segment_id = segment_id;
    header.mode = VECTOR_WAL_SYNC;

    /* 写入头部 */
    if (fwrite(&header, sizeof(header), 1, wal->fp) != 1) {
        LOG_ERROR("写入 WAL 头部失败");
        free(wal->buffer);
        fclose(wal->fp);
        free(wal);
        return NULL;
    }

    wal->buffer_size = VECTOR_WAL_BUFFER_SIZE;
    wal->buffer_used = 0;
    wal->num_records = 0;
    wal->num_vectors = 0;
    wal->last_checkpoint = sizeof(header);
    wal->mode = VECTOR_WAL_SYNC;
    wal->is_open = true;
    pthread_mutex_init(&wal->mutex, NULL);

    LOG_INFO("创建 WAL 成功: %s", wal->path);
    return wal;
}

/**
 * @brief 打开已有 WAL
 */
vector_wal_t *vector_wal_open(const char *path, uint32_t segment_id) {
    if (!path) {
        LOG_ERROR("WAL 路径为空");
        return NULL;
    }

    vector_wal_t *wal = (vector_wal_t *)calloc(1, sizeof(vector_wal_t));
    if (!wal) {
        LOG_ERROR("分配 WAL 失败");
        return NULL;
    }

    wal_make_path(wal->path, sizeof(wal->path), path, segment_id);

    wal->fp = fopen(wal->path, "r+b");
    if (!wal->fp) {
        /* WAL 不存在，需要创建 */
        free(wal);
        return vector_wal_create(path, segment_id);
    }

    /* 读取头部 */
    vector_wal_file_header_t header;
    if (fread(&header, sizeof(header), 1, wal->fp) != 1) {
        LOG_ERROR("读取 WAL 头部失败");
        fclose(wal->fp);
        free(wal);
        return NULL;
    }

    if (header.magic != VECTOR_WAL_MAGIC) {
        LOG_ERROR("WAL 魔数不匹配: 0x%X != 0x%X", header.magic, VECTOR_WAL_MAGIC);
        fclose(wal->fp);
        free(wal);
        return NULL;
    }

    /* 分配缓冲区 */
    wal->buffer = (uint8_t *)malloc(VECTOR_WAL_BUFFER_SIZE);
    if (!wal->buffer) {
        LOG_ERROR("分配 WAL 缓冲区失败");
        fclose(wal->fp);
        free(wal);
        return NULL;
    }

    wal->buffer_size = VECTOR_WAL_BUFFER_SIZE;
    wal->buffer_used = 0;
    wal->num_records = header.num_records;
    wal->num_vectors = header.num_vectors;
    wal->last_checkpoint = header.last_checkpoint;
    wal->mode = (vector_wal_mode_t)header.mode;
    wal->is_open = true;
    pthread_mutex_init(&wal->mutex, NULL);

    /* 跳到文件末尾 */
    fseek(wal->fp, 0, SEEK_END);

    LOG_INFO("打开 WAL 成功: %s, records=%lu", wal->path, wal->num_records);
    return wal;
}

/**
 * @brief 关闭 WAL
 */
int vector_wal_close(vector_wal_t *wal) {
    if (!wal) return 0;

    pthread_mutex_lock(&wal->mutex);

    if (wal->is_open) {
        /* 刷新缓冲区 */
        if (wal->buffer_used > 0) {
            fwrite(wal->buffer, wal->buffer_used, 1, wal->fp);
            fflush(wal->fp);
        }

        /* 更新头部 */
        fseek(wal->fp, 0, SEEK_SET);
        vector_wal_file_header_t header;
        if (fread(&header, sizeof(header), 1, wal->fp) == 1) {
            header.num_records = wal->num_records;
            header.num_vectors = wal->num_vectors;
            header.last_checkpoint = wal->last_checkpoint;
            fseek(wal->fp, 0, SEEK_SET);
            fwrite(&header, sizeof(header), 1, wal->fp);
        }

        if (wal->fp) {
            fclose(wal->fp);
            wal->fp = NULL;
        }
        wal->is_open = false;
    }

    pthread_mutex_unlock(&wal->mutex);
    pthread_mutex_destroy(&wal->mutex);

    if (wal->buffer) {
        free(wal->buffer);
        wal->buffer = NULL;
    }

    free(wal);
    LOG_INFO("关闭 WAL 成功");
    return 0;
}

/**
 * @brief 删除 WAL
 */
int vector_wal_drop(const char *path, uint32_t segment_id) {
    char wal_path[256];
    wal_make_path(wal_path, sizeof(wal_path), path, segment_id);

    if (remove(wal_path) == 0) {
        LOG_INFO("删除 WAL: %s", wal_path);
        return 0;
    }
    return -1;
}

/* ============================================================
 * WAL 写入
 * ============================================================ */

/**
 * @brief 写入追加向量记录
 */
int vector_wal_append(vector_wal_t *wal, uint32_t segment_id,
                      int32_t vector_id, int32_t dims, const float *vector) {
    if (!wal || !wal->is_open || !vector) {
        LOG_ERROR("无效参数");
        return -1;
    }

    pthread_mutex_lock(&wal->mutex);

    /* 序列化记录 */
    uint8_t record[VECTOR_WAL_RECORD_HEADER_SIZE + dims * sizeof(float)];
    size_t data_size = dims * sizeof(float);

    /* 写入记录头 */
    record[0] = VECTOR_WAL_APPEND;
    memcpy(&record[1], &segment_id, 4);
    memcpy(&record[5], &vector_id, 4);
    memcpy(&record[9], &dims, 4);
    memcpy(&record[13], &data_size, 4);

    /* 写入向量数据 */
    if (dims > 0 && vector) {
        memcpy(&record[VECTOR_WAL_RECORD_HEADER_SIZE], vector, data_size);
    }

    /* 计算校验和 */
    uint32_t crc = wal_crc32(record, sizeof(record) - 4);
    memcpy(&record[sizeof(record) - 4], &crc, 4);

    /* 写入缓冲区或直接刷盘 */
    size_t record_size = sizeof(record);
    if (wal->mode == VECTOR_WAL_SYNC) {
        /* 同步模式：直接写入并刷盘 */
        if (fwrite(record, record_size, 1, wal->fp) != 1) {
            LOG_ERROR("写入 WAL 记录失败");
            pthread_mutex_unlock(&wal->mutex);
            return -1;
        }
        fflush(wal->fp);
    } else {
        /* 异步模式：写入缓冲区 */
        if (wal->buffer_used + record_size > wal->buffer_size) {
            /* 缓冲区满，先刷盘 */
            fwrite(wal->buffer, wal->buffer_used, 1, wal->fp);
            wal->buffer_used = 0;
        }
        memcpy(wal->buffer + wal->buffer_used, record, record_size);
        wal->buffer_used += record_size;
    }

    wal->num_records++;
    wal->num_vectors++;

    pthread_mutex_unlock(&wal->mutex);
    return 0;
}

/**
 * @brief 写入删除向量记录
 */
int vector_wal_delete(vector_wal_t *wal, uint32_t segment_id, int32_t vector_id) {
    if (!wal || !wal->is_open) {
        LOG_ERROR("无效参数");
        return -1;
    }

    pthread_mutex_lock(&wal->mutex);

    /* 序列化记录 */
    uint8_t record[VECTOR_WAL_RECORD_HEADER_SIZE];

    record[0] = VECTOR_WAL_DELETE;
    memset(&record[1], 0, 4);  /* segment_id */
    memcpy(&record[1], &segment_id, 4);
    memcpy(&record[5], &vector_id, 4);
    int32_t zero_dims = 0;
    memcpy(&record[9], &zero_dims, 4);
    uint32_t zero_size = 0;
    memcpy(&record[13], &zero_size, 4);

    /* 计算校验和 */
    uint32_t crc = wal_crc32(record, sizeof(record) - 4);
    memcpy(&record[sizeof(record) - 4], &crc, 4);

    /* 写入 */
    if (wal->mode == VECTOR_WAL_SYNC) {
        if (fwrite(record, sizeof(record), 1, wal->fp) != 1) {
            LOG_ERROR("写入 WAL 记录失败");
            pthread_mutex_unlock(&wal->mutex);
            return -1;
        }
        fflush(wal->fp);
    } else {
        if (wal->buffer_used + sizeof(record) > wal->buffer_size) {
            fwrite(wal->buffer, wal->buffer_used, 1, wal->fp);
            wal->buffer_used = 0;
        }
        memcpy(wal->buffer + wal->buffer_used, record, sizeof(record));
        wal->buffer_used += sizeof(record);
    }

    wal->num_records++;

    pthread_mutex_unlock(&wal->mutex);
    return 0;
}

/**
 * @brief 刷新 WAL 缓冲区
 */
int vector_wal_flush(vector_wal_t *wal) {
    if (!wal || !wal->is_open) {
        return -1;
    }

    pthread_mutex_lock(&wal->mutex);

    if (wal->buffer_used > 0) {
        if (fwrite(wal->buffer, wal->buffer_used, 1, wal->fp) != 1) {
            LOG_ERROR("刷新 WAL 缓冲区失败");
            pthread_mutex_unlock(&wal->mutex);
            return -1;
        }
        fflush(wal->fp);
        wal->buffer_used = 0;
    }

    pthread_mutex_unlock(&wal->mutex);
    return 0;
}

/* ============================================================
 * Checkpoint
 * ============================================================ */

/**
 * @brief 执行检查点
 */
int vector_wal_checkpoint(vector_wal_t *wal, vector_checkpoint_type_t type) {
    if (!wal || !wal->is_open) {
        LOG_ERROR("WAL 未打开");
        return -1;
    }

    pthread_mutex_lock(&wal->mutex);

    /* 刷新缓冲区 */
    if (wal->buffer_used > 0) {
        fwrite(wal->buffer, wal->buffer_used, 1, wal->fp);
        wal->buffer_used = 0;
    }

    /* 同步文件 */
    fflush(wal->fp);

    /* 获取当前位置 */
    long pos = ftell(wal->fp);
    if (pos < 0) {
        LOG_ERROR("获取 WAL 位置失败");
        pthread_mutex_unlock(&wal->mutex);
        return -1;
    }
    wal->last_checkpoint = (uint64_t)pos;

    /* 更新头部 */
    fseek(wal->fp, 0, SEEK_SET);
    vector_wal_file_header_t header;
    if (fread(&header, sizeof(header), 1, wal->fp) == 1) {
        header.last_checkpoint = wal->last_checkpoint;
        header.num_records = wal->num_records;
        header.num_vectors = wal->num_vectors;
        fseek(wal->fp, 0, SEEK_SET);
        fwrite(&header, sizeof(header), 1, wal->fp);
        fflush(wal->fp);
    }

    /* 跳回文件末尾 */
    fseek(wal->fp, 0, SEEK_END);

    pthread_mutex_unlock(&wal->mutex);

    const char *type_str[] = {"shutdown", "manual", "timed"};
    LOG_INFO("执行 checkpoint 成功: type=%s, offset=%lu",
             type_str[type], wal->last_checkpoint);
    return 0;
}

/**
 * @brief 截断 WAL 到检查点
 */
int vector_wal_truncate(vector_wal_t *wal) {
    if (!wal || !wal->is_open) {
        return -1;
    }

    pthread_mutex_lock(&wal->mutex);

    if (wal->last_checkpoint > VECTOR_WAL_HEADER_SIZE) {
        /* 截断文件 - 先关闭再重新创建 */
        char temp_path[256];
        snprintf(temp_path, sizeof(temp_path), "%s.tmp", wal->path);

        FILE *src = wal->fp;
        FILE *dst = fopen(temp_path, "wb");
        if (!dst) {
            LOG_ERROR("创建临时文件失败");
            pthread_mutex_unlock(&wal->mutex);
            return -1;
        }

        /* 复制到检查点位置 */
        fseek(src, 0, SEEK_SET);
        uint8_t buf[4096];
        long remaining = (long)wal->last_checkpoint;
        while (remaining > 0) {
            size_t to_read = remaining > (long)sizeof(buf) ? sizeof(buf) : (size_t)remaining;
            size_t read = fread(buf, 1, to_read, src);
            if (read == 0) break;
            fwrite(buf, 1, read, dst);
            remaining -= (long)read;
        }
        fclose(dst);
        fclose(src);

        /* 替换原文件 */
        remove(wal->path);
        rename(temp_path, wal->path);

        /* 重新打开文件 */
        wal->fp = fopen(wal->path, "r+b");
        if (!wal->fp) {
            LOG_ERROR("重新打开 WAL 失败");
            pthread_mutex_unlock(&wal->mutex);
            return -1;
        }

        wal->num_records = 0;
        wal->num_vectors = 0;
        wal->buffer_used = 0;
    }

    pthread_mutex_unlock(&wal->mutex);
    LOG_INFO("截断 WAL 成功: checkpoint=%lu", wal->last_checkpoint);
    return 0;
}

/* ============================================================
 * 查询接口
 * ============================================================ */

/**
 * @brief 获取 WAL 记录数
 */
uint64_t vector_wal_num_records(const vector_wal_t *wal) {
    return wal ? wal->num_records : 0;
}

/**
 * @brief 获取 WAL 文件大小
 */
uint64_t vector_wal_file_size(const vector_wal_t *wal) {
    if (!wal) return 0;

    struct stat st;
    if (stat(wal->path, &st) == 0) {
        return (uint64_t)st.st_size;
    }
    return 0;
}

/* ============================================================
 * WAL 重放
 * ============================================================ */

/**
 * @brief 重放 WAL 到向量段
 */
int64_t vector_wal_replay(const char *wal_path, uint32_t segment_id,
                          vector_segment_t *segment,
                          bool (*callback)(int64_t progress, int64_t total, void *user_data),
                          void *user_data) {
    char path[256];
    wal_make_path(path, sizeof(path), wal_path, segment_id);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        LOG_WARN("WAL 文件不存在: %s", path);
        return 0;
    }

    /* 读取头部 */
    vector_wal_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        LOG_ERROR("读取 WAL 头部失败");
        fclose(fp);
        return -1;
    }

    if (header.magic != VECTOR_WAL_MAGIC) {
        LOG_ERROR("WAL 魔数不匹配");
        fclose(fp);
        return -1;
    }

    LOG_INFO("开始重放 WAL: records=%lu", header.num_records);

    /* 跳到检查点位置 */
    if (header.last_checkpoint > VECTOR_WAL_HEADER_SIZE) {
        fseek(fp, header.last_checkpoint, SEEK_SET);
    }

    /* 读取记录 */
    uint8_t record[1024];
    int64_t count = 0;
    int64_t total = header.num_records;

    while (fread(record, VECTOR_WAL_RECORD_HEADER_SIZE, 1, fp) == 1) {
        /* 解析记录头 */
        vector_wal_op_t op = (vector_wal_op_t)record[0];
        uint32_t seg_id;
        int32_t vec_id, dims;
        uint32_t data_size;

        memcpy(&seg_id, &record[1], 4);
        memcpy(&vec_id, &record[5], 4);
        memcpy(&dims, &record[9], 4);
        memcpy(&data_size, &record[13], 4);

        /* 读取数据 */
        if (data_size > 0 && data_size <= sizeof(record) - VECTOR_WAL_RECORD_HEADER_SIZE) {
            if (fread(record + VECTOR_WAL_RECORD_HEADER_SIZE, data_size, 1, fp) != 1) {
                break;
            }
        }

        /* 验证校验和 */
        uint32_t stored_crc, calc_crc;
        memcpy(&stored_crc, &record[VECTOR_WAL_RECORD_HEADER_SIZE + data_size - 4], 4);
        calc_crc = wal_crc32(record, VECTOR_WAL_RECORD_HEADER_SIZE + data_size - 4);
        if (stored_crc != calc_crc) {
            LOG_WARN("WAL 记录校验和不匹配，跳过");
            continue;
        }

        /* 重放操作 */
        if (op == VECTOR_WAL_APPEND && segment) {
            float *vec = (float *)(record + VECTOR_WAL_RECORD_HEADER_SIZE);
            vector_segment_append(segment, vec);
        } else if (op == VECTOR_WAL_DELETE && segment) {
            vector_segment_delete(segment, vec_id);
        }

        count++;

        /* 回调 */
        if (callback && !callback(count, total, user_data)) {
            break;
        }
    }

    fclose(fp);
    LOG_INFO("WAL 重放完成: count=%ld", count);
    return count;
}

/* ============================================================
 * 崩溃恢复流程设计
 * ============================================================ */

/**
 * @page vector_recovery_design 崩溃恢复流程设计
 *
 * 恢复流程分为以下几个阶段：
 *
 * 1. 检测阶段
 *    - 检查 WAL 文件是否存在
 *    - 读取 WAL 头部获取元数据
 *    - 验证 WAL 文件完整性
 *
 * 2. 分析阶段
 *    - 扫描 WAL 记录
 *    - 确定最后检查点位置
 *    - 识别未完成的操作
 *
 * 3. 恢复阶段
 *    - 重放检查点后的 WAL 记录
 *    - 应用每个操作到数据文件
 *    - 记录恢复进度
 *
 * 4. 清理阶段
 *    - 截断 WAL 到检查点
 *    - 更新检查点信息
 *    - 生成恢复报告
 *
 * 恢复策略：
 * - 如果数据文件完整：重放 WAL 恢复丢失的操作
 * - 如果数据文件损坏：从 WAL 重建数据文件
 * - 如果 WAL 损坏：回退到最近的检查点
 */

/* ============================================================
 * 崩溃恢复实现
 * ============================================================ */

/**
 * @brief 获取检查点信息
 */
int vector_wal_get_checkpoint_info(const char *wal_path, uint32_t segment_id,
                                   uint64_t *out_checkpoint, uint64_t *out_records) {
    char path[256];
    wal_make_path(path, sizeof(path), wal_path, segment_id);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }

    vector_wal_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (out_checkpoint) *out_checkpoint = header.last_checkpoint;
    if (out_records) *out_records = header.num_records;

    fclose(fp);
    return 0;
}

/**
 * @brief 恢复向量索引
 */
vector_recovery_result_t *vector_index_recover(const char *data_dir, uint32_t segment_id) {
    vector_recovery_result_t *result = (vector_recovery_result_t *)calloc(1, sizeof(vector_recovery_result_t));
    if (!result) return NULL;

    uint64_t start_time = (uint64_t)clock();

    /* 构造路径 */
    char wal_path[256];
    char segment_path[256];
    snprintf(wal_path, sizeof(wal_path), "%s/wal", data_dir);
    snprintf(segment_path, sizeof(segment_path), "%s/segment_%u", data_dir, segment_id);

    /* 检查 WAL 是否存在 */
    char full_wal_path[256];
    wal_make_path(full_wal_path, sizeof(full_wal_path), wal_path, segment_id);

    FILE *fp = fopen(full_wal_path, "rb");
    if (!fp) {
        result->status = VECTOR_RECOVERY_NO_WAL;
        result->elapsed_ms = ((uint64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;
        snprintf(result->error_msg, sizeof(result->error_msg), "WAL 文件不存在");
        LOG_INFO("无 WAL 文件需要恢复: %s", full_wal_path);
        return result;
    }

    /* 读取 WAL 头部 */
    vector_wal_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        result->status = VECTOR_RECOVERY_CORRUPTED;
        result->elapsed_ms = ((uint64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;
        snprintf(result->error_msg, sizeof(result->error_msg), "无法读取 WAL 头部");
        fclose(fp);
        return result;
    }

    if (header.magic != VECTOR_WAL_MAGIC) {
        result->status = VECTOR_RECOVERY_CORRUPTED;
        result->elapsed_ms = ((uint64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;
        snprintf(result->error_msg, sizeof(result->error_msg), "WAL 魔数不匹配");
        fclose(fp);
        return result;
    }

    LOG_INFO("开始恢复: segment_id=%u, records=%lu, checkpoint=%lu",
             segment_id, header.num_records, header.last_checkpoint);

    /* 跳到检查点位置 */
    if (header.last_checkpoint > VECTOR_WAL_HEADER_SIZE) {
        fseek(fp, header.last_checkpoint, SEEK_SET);
    }

    /* 打开向量段 */
    vector_segment_t *segment = vector_segment_open(segment_path, segment_id);
    if (!segment) {
        /* 段不存在，需要创建 */
        /* 从 WAL 头部获取维度信息（如果有） */
        segment = vector_segment_create(segment_path, 128, segment_id);
    }

    if (!segment) {
        result->status = VECTOR_RECOVERY_FAILED;
        result->elapsed_ms = ((uint64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;
        snprintf(result->error_msg, sizeof(result->error_msg), "无法打开向量段");
        fclose(fp);
        return result;
    }

    /* 读取并重放记录 */
    uint8_t record[1024];
    uint64_t vectors_count = 0;
    uint64_t records_count = 0;

    while (fread(record, VECTOR_WAL_RECORD_HEADER_SIZE, 1, fp) == 1) {
        vector_wal_op_t op = (vector_wal_op_t)record[0];
        int32_t vec_id, dims;
        uint32_t data_size;

        memcpy(&vec_id, &record[5], 4);
        memcpy(&dims, &record[9], 4);
        memcpy(&data_size, &record[13], 4);

        /* 读取数据 */
        if (data_size > 0 && data_size <= sizeof(record) - VECTOR_WAL_RECORD_HEADER_SIZE) {
            if (fread(record + VECTOR_WAL_RECORD_HEADER_SIZE, data_size, 1, fp) != 1) {
                break;
            }
        }

        /* 验证校验和 */
        size_t rec_size = VECTOR_WAL_RECORD_HEADER_SIZE + data_size;
        uint32_t stored_crc, calc_crc;
        memcpy(&stored_crc, &record[rec_size - 4], 4);
        calc_crc = wal_crc32(record, rec_size - 4);
        if (stored_crc != calc_crc) {
            LOG_WARN("WAL 记录校验和不匹配，跳过");
            continue;
        }

        /* 重放操作 */
        if (op == VECTOR_WAL_APPEND) {
            float *vec = (float *)(record + VECTOR_WAL_RECORD_HEADER_SIZE);
            if (vector_segment_append(segment, vec) >= 0) {
                vectors_count++;
            }
        } else if (op == VECTOR_WAL_DELETE) {
            vector_segment_delete(segment, vec_id);
        }

        records_count++;
    }

    fclose(fp);
    vector_segment_close(segment);

    result->status = VECTOR_RECOVERY_OK;
    result->records_recovered = records_count;
    result->vectors_recovered = vectors_count;
    result->elapsed_ms = ((uint64_t)clock() - start_time) * 1000 / CLOCKS_PER_SEC;

    LOG_INFO("恢复完成: records=%lu, vectors=%lu, elapsed=%lums",
             records_count, vectors_count, result->elapsed_ms);
    return result;
}

/**
 * @brief 释放恢复结果
 */
void vector_recovery_result_free(vector_recovery_result_t *result) {
    if (result) {
        free(result);
    }
}

/**
 * @brief 获取恢复状态描述
 */
const char *vector_recovery_status_str(vector_recovery_status_t status) {
    switch (status) {
        case VECTOR_RECOVERY_OK:        return "恢复成功";
        case VECTOR_RECOVERY_NO_WAL:    return "无 WAL 文件";
        case VECTOR_RECOVERY_CORRUPTED: return "WAL 文件损坏";
        case VECTOR_RECOVERY_FAILED:    return "恢复失败";
        default:                        return "未知状态";
    }
}

/**
 * @brief 打印恢复报告
 */
void vector_recovery_print_report(const vector_recovery_result_t *result) {
    if (!result) return;

    printf("\n");
    printf("========================================\n");
    printf("        崩溃恢复报告\n");
    printf("========================================\n");
    printf("状态: %s\n", vector_recovery_status_str(result->status));
    printf("恢复记录数: %lu\n", (unsigned long)result->records_recovered);
    printf("恢复向量数: %lu\n", (unsigned long)result->vectors_recovered);
    printf("恢复耗时: %lu ms\n", (unsigned long)result->elapsed_ms);
    if (result->error_msg[0]) {
        printf("错误信息: %s\n", result->error_msg);
    }
    printf("========================================\n");
    printf("\n");
}

/* ============================================================
 * 序列化/反序列化 (兼容旧 API)
 * ============================================================ */

/**
 * @brief 序列化向量 WAL 记录
 */
size_t vector_wal_serialize(vector_wal_op_t op, uint32_t segment_id,
                            int32_t vector_id, int32_t dims,
                            const float *data, void *out_buf, size_t buf_size) {
    size_t header_size = VECTOR_WAL_RECORD_HEADER_SIZE;
    size_t data_size = (size_t)dims * sizeof(float);
    size_t total = header_size + data_size + 4;  /* +4 for CRC */

    if (buf_size < total || !out_buf) {
        return 0;
    }

    uint8_t *buf = (uint8_t *)out_buf;
    buf[0] = (uint8_t)op;
    memcpy(&buf[1], &segment_id, 4);
    memcpy(&buf[5], &vector_id, 4);
    memcpy(&buf[9], &dims, 4);
    memcpy(&buf[13], &data_size, 4);

    if (data && data_size > 0) {
        memcpy(&buf[header_size], data, data_size);
    }

    uint32_t crc = wal_crc32(buf, total - 4);
    memcpy(&buf[total - 4], &crc, 4);

    return total;
}

/**
 * @brief 反序列化向量 WAL 记录
 */
int vector_wal_deserialize(const void *buf, size_t buf_size,
                           vector_wal_op_t *out_op, uint32_t *out_segment_id,
                           int32_t *out_vector_id, int32_t *out_dims,
                           float *out_data) {
    if (!buf || buf_size < VECTOR_WAL_RECORD_HEADER_SIZE) {
        return -1;
    }

    const uint8_t *p = (const uint8_t *)buf;

    if (out_op) *out_op = (vector_wal_op_t)p[0];
    if (out_segment_id) memcpy(out_segment_id, &p[1], 4);
    if (out_vector_id) memcpy(out_vector_id, &p[5], 4);
    if (out_dims) memcpy(out_dims, &p[9], 4);

    uint32_t data_size;
    memcpy(&data_size, &p[13], 4);

    if (out_data && data_size > 0) {
        size_t header_size = VECTOR_WAL_RECORD_HEADER_SIZE;
        if (buf_size >= header_size + data_size) {
            memcpy(out_data, &p[header_size], data_size);
        }
    }

    /* 验证校验和 */
    uint32_t stored_crc, calc_crc;
    memcpy(&stored_crc, &p[buf_size - 4], 4);
    calc_crc = wal_crc32(p, buf_size - 4);
    if (stored_crc != calc_crc) {
        LOG_WARN("WAL 记录校验和不匹配");
        return -1;
    }

    return 0;
}
