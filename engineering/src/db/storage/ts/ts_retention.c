/**
 * @file ts_retention.c
 * @brief 时序数据保留策略实现
 *
 * 支持多级保留策略、数据降采样和自动清理。
 */
#include "db/storage/ts/ts_retention.h"
#include "db/storage/ts/ts_engine.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* 平台特定的目录和文件操作头文件 */
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define RETENTION_META_SUFFIX ".retention"

/* ========================================================================
 * 时间转换工具
 * ======================================================================== */

int64_t ts_days_to_ms(int64_t days) {
    return days * 24LL * 60 * 60 * 1000;
}

int64_t ts_hours_to_ms(int64_t hours) {
    return hours * 60LL * 60 * 1000;
}

int64_t ts_timestamp_minus_days(int64_t days) {
    int64_t now_ms = (int64_t)time(NULL) * 1000;
    return now_ms - ts_days_to_ms(days);
}

/* ========================================================================
 * 当前时间戳（毫秒）
 * ======================================================================== */

static int64_t ts_now_ms(void) {
    return (int64_t)time(NULL) * 1000;
}

/* ========================================================================
 * 保留策略默认值
 * ======================================================================== */

ts_retention_policy_t ts_retention_default_policy(void) {
    ts_retention_policy_t policy = {
        .raw_retention_days = TS_RETENTION_DEFAULT_RAW_DAYS,
        .compressed_retention_days = TS_RETENTION_DEFAULT_COMPRESSED_DAYS,
        .compaction_interval_hours = TS_RETENTION_DEFAULT_COMPACTION_INTERVAL_HOURS,
        /* 新增：多级保留默认值 */
        .default_tier = TS_RETENTION_HOT,
        .hot_ttl_ms = TS_DAYS_TO_MS(TS_RETENTION_DEFAULT_HOT_DAYS),
        .warm_ttl_ms = TS_DAYS_TO_MS(TS_RETENTION_DEFAULT_WARM_DAYS),
        .cold_ttl_ms = TS_DAYS_TO_MS(TS_RETENTION_DEFAULT_COLD_DAYS),
        .downsample_interval_ms = 0,
        .downsample_func = TS_DOWNSAMPLE_AVG,
        .cleanup_interval_ms = 60 * 60 * 1000,  /* 1 小时 */
        .last_cleanup_time = 0
    };
    return policy;
}

bool ts_retention_policy_valid(const ts_retention_policy_t *policy) {
    if (policy == NULL) return false;
    /* 负数（除 -1 表示无限）和过大值无效 */
    if (policy->raw_retention_days < -1) return false;
    if (policy->compressed_retention_days < -1) return false;
    if (policy->compaction_interval_hours <= 0) return false;
    /* 验证多级 TTL */
    if (policy->hot_ttl_ms < 0 || policy->warm_ttl_ms < 0 || policy->cold_ttl_ms < 0) {
        return false;
    }
    return true;
}

/* ========================================================================
 * 保留策略创建
 * ======================================================================== */

ts_retention_policy_t *ts_retention_policy_create(
    int32_t hot_ttl_days,
    int32_t warm_ttl_days,
    int32_t cold_ttl_days) {

    ts_retention_policy_t *policy =
        (ts_retention_policy_t *)malloc(sizeof(ts_retention_policy_t));
    if (!policy) {
        LOG_ERROR("分配保留策略内存失败");
        return NULL;
    }

    *policy = ts_retention_default_policy();

    if (hot_ttl_days >= 0) {
        policy->hot_ttl_ms = ts_days_to_ms(hot_ttl_days);
        policy->raw_retention_days = hot_ttl_days;
    }
    if (warm_ttl_days >= 0) {
        policy->warm_ttl_ms = ts_days_to_ms(warm_ttl_days);
    }
    if (cold_ttl_days >= 0) {
        policy->cold_ttl_ms = ts_days_to_ms(cold_ttl_days);
    }

    return policy;
}

void ts_retention_policy_free(ts_retention_policy_t *policy) {
    if (policy) {
        free(policy);
    }
}

/* ========================================================================
 * 保留策略持久化
 * ======================================================================== */

static int get_retention_meta_path(const char *data_dir, const char *name,
                                    char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s%s",
             data_dir, "ts_", name, RETENTION_META_SUFFIX);
    return 0;
}

int ts_retention_load_policy(const char *data_dir, const char *name,
                              ts_retention_policy_t *policy) {
    if (data_dir == NULL || name == NULL || policy == NULL) return -1;

    char path[512];
    get_retention_meta_path(data_dir, name, path, sizeof(path));

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        /* 文件不存在，使用默认值 */
        *policy = ts_retention_default_policy();
        return 0;
    }

    if (fread(policy, sizeof(ts_retention_policy_t), 1, fp) != 1) {
        fclose(fp);
        *policy = ts_retention_default_policy();
        return -1;
    }

    fclose(fp);
    return 0;
}

int ts_retention_policy_save(const char *data_dir, const char *name,
                              const ts_retention_policy_t *policy) {
    if (data_dir == NULL || name == NULL || policy == NULL) return -1;

    char path[512];
    get_retention_meta_path(data_dir, name, path, sizeof(path));

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        LOG_ERROR("无法保存保留策略到文件: %s", path);
        return -1;
    }

    if (fwrite(policy, sizeof(ts_retention_policy_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

/* ========================================================================
 * 多级保留策略 API
 * ======================================================================== */

/** 计算指定层级的保留截止时间 */
int64_t ts_retention_cutoff_time(ts_retention_tier_t tier,
                                  const ts_retention_policy_t *policy) {
    if (!policy) return 0;

    int64_t ttl_ms;
    switch (tier) {
        case TS_RETENTION_HOT:
            ttl_ms = policy->hot_ttl_ms;
            break;
        case TS_RETENTION_WARM:
            ttl_ms = policy->warm_ttl_ms;
            break;
        case TS_RETENTION_COLD:
            ttl_ms = policy->cold_ttl_ms;
            break;
        default:
            return 0;
    }

    /* TTL 为 0 表示永久保留 */
    if (ttl_ms <= 0) {
        return 0;
    }

    return ts_now_ms() - ttl_ms;
}

/** 检查是否需要清理 */
bool ts_retention_need_cleanup(int64_t last_timestamp,
                               ts_retention_tier_t tier,
                               const ts_retention_policy_t *policy,
                               int64_t now_ms) {
    if (!policy) return false;

    int64_t cutoff = ts_retention_cutoff_time(tier, policy);
    if (cutoff == 0) return false;  /* 永久保留 */

    return last_timestamp < cutoff;
}

/**
 * @brief 扫描数据目录中的 .tsd 文件并删除过期文件
 *
 * @param data_dir   数据目录
 * @param policy     保留策略
 * @param now_ms     当前时间戳（毫秒）
 * @param result     输出清理结果
 * @return 删除的文件数，失败返回 -1
 */
static int scan_and_cleanup_files(const char *data_dir,
                                   const ts_retention_policy_t *policy,
                                   int64_t now_ms,
                                   ts_cleanup_result_t *result) {
    if (!data_dir || !policy) return -1;

    int deleted_count = 0;
    int64_t hot_cutoff = ts_retention_cutoff_time(TS_RETENTION_HOT, policy);
    int64_t warm_cutoff = ts_retention_cutoff_time(TS_RETENTION_WARM, policy);
    int64_t cold_cutoff = ts_retention_cutoff_time(TS_RETENTION_COLD, policy);

    /* 构建搜索路径 */
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s/*.tsd", data_dir);

    /* 使用平台特定的目录扫描 */
#ifdef _WIN32
    /* Windows: 使用 _findfirst/_findnext */
    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(search_path, &fileinfo);
    if (handle == -1) {
        LOG_INFO("数据目录为空或不存在: %s", data_dir);
        return 0;
    }

    do {
        /* 跳过目录 */
        if (fileinfo.attrib & _A_SUBDIR) continue;

        /* 获取文件完整路径 */
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", data_dir, fileinfo.name);

        /* 获取文件修改时间（毫秒） */
        int64_t file_mtime_ms = (int64_t)fileinfo.time_write * 1000;

        /* 判断文件是否过期 */
        bool expired = false;

        /* 热层过期：文件修改时间早于 hot_cutoff */
        if (hot_cutoff > 0 && file_mtime_ms < hot_cutoff) {
            expired = true;
            LOG_INFO("文件 %s 已过热层保留期，将删除", fileinfo.name);
        }
        /* 温层过期：文件修改时间早于 warm_cutoff */
        else if (warm_cutoff > 0 && file_mtime_ms < warm_cutoff) {
            expired = true;
            LOG_INFO("文件 %s 已过温层保留期，将删除", fileinfo.name);
        }
        /* 冷层过期：文件修改时间早于 cold_cutoff */
        else if (cold_cutoff > 0 && file_mtime_ms < cold_cutoff) {
            expired = true;
            LOG_INFO("文件 %s 已过冷层保留期，将删除", fileinfo.name);
        }

        /* 删除过期文件 */
        if (expired) {
            if (remove(filepath) == 0) {
                deleted_count++;
                LOG_INFO("已删除过期文件: %s", filepath);
            } else {
                LOG_ERROR("删除文件失败: %s", filepath);
            }
        }
    } while (_findnext(handle, &fileinfo) == 0);

    _findclose(handle);
#else
    /* POSIX: 使用 opendir/readdir */
    DIR *dir = opendir(data_dir);
    if (!dir) {
        LOG_INFO("无法打开数据目录: %s", data_dir);
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* 跳过目录和非 .tsd 文件 */
        if (entry->d_type == DT_DIR) continue;

        const char *name = entry->d_name;
        size_t name_len = strlen(name);
        if (name_len < 4 || strcmp(name + name_len - 4, ".tsd") != 0) {
            continue;
        }

        /* 获取文件完整路径 */
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", data_dir, name);

        /* 获取文件状态 */
        struct stat file_stat;
        if (stat(filepath, &file_stat) != 0) {
            LOG_ERROR("获取文件状态失败: %s", filepath);
            continue;
        }

        /* 获取文件修改时间（毫秒） */
        int64_t file_mtime_ms = (int64_t)file_stat.st_mtime * 1000;

        /* 判断文件是否过期 */
        bool expired = false;

        /* 热层过期 */
        if (hot_cutoff > 0 && file_mtime_ms < hot_cutoff) {
            expired = true;
            LOG_INFO("文件 %s 已过热层保留期，将删除", name);
        }
        /* 温层过期 */
        else if (warm_cutoff > 0 && file_mtime_ms < warm_cutoff) {
            expired = true;
            LOG_INFO("文件 %s 已过温层保留期，将删除", name);
        }
        /* 冷层过期 */
        else if (cold_cutoff > 0 && file_mtime_ms < cold_cutoff) {
            expired = true;
            LOG_INFO("文件 %s 已过冷层保留期，将删除", name);
        }

        /* 删除过期文件 */
        if (expired) {
            if (remove(filepath) == 0) {
                deleted_count++;
                LOG_INFO("已删除过期文件: %s", filepath);
            } else {
                LOG_ERROR("删除文件失败: %s", filepath);
            }
        }
    }

    closedir(dir);
#endif

    return deleted_count;
}

/** 执行数据清理 */
int ts_retention_cleanup(const char *data_dir,
                         ts_retention_policy_t *policy,
                         int64_t now_ms,
                         ts_cleanup_result_t *result) {
    if (!data_dir || !policy) return -1;

    if (result) {
        memset(result, 0, sizeof(*result));
    }

    int64_t start_time = ts_now_ms();

    /* 检查是否到达清理时间 */
    if (policy->last_cleanup_time > 0) {
        int64_t elapsed = now_ms - policy->last_cleanup_time;
        if (elapsed < policy->cleanup_interval_ms) {
            if (result) {
                result->next_cleanup_time = policy->last_cleanup_time + policy->cleanup_interval_ms;
            }
            return 0;  /* 未到清理时间 */
        }
    }

    LOG_INFO("开始执行时序数据清理...");

    /* 计算各层截止时间 */
    int64_t hot_cutoff = ts_retention_cutoff_time(TS_RETENTION_HOT, policy);
    int64_t warm_cutoff = ts_retention_cutoff_time(TS_RETENTION_WARM, policy);
    int64_t cold_cutoff = ts_retention_cutoff_time(TS_RETENTION_COLD, policy);

    (void)hot_cutoff;
    (void)warm_cutoff;
    (void)cold_cutoff;

    /* 扫描并清理过期文件 */
    int deleted_count = scan_and_cleanup_files(data_dir, policy, now_ms, result);
    if (deleted_count < 0) {
        LOG_ERROR("文件扫描清理失败");
        return -1;
    }

    /* 更新清理时间 */
    policy->last_cleanup_time = now_ms;

    if (result) {
        result->duration_ms = ts_now_ms() - start_time;
        result->next_cleanup_time = now_ms + policy->cleanup_interval_ms;
    }

    LOG_INFO("时序数据清理完成，删除 %d 个过期文件，耗时 %lld ms",
             deleted_count, (long long)(result ? result->duration_ms : 0));

    return 0;
}

/* ========================================================================
 * 降采样 API
 * ======================================================================== */

/** 获取降采样函数名称 */
const char *ts_downsampling_func_name(ts_downsampling_func_t func) {
    switch (func) {
        case TS_DOWNSAMPLE_AVG:   return "AVG";
        case TS_DOWNSAMPLE_MIN:   return "MIN";
        case TS_DOWNSAMPLE_MAX:   return "MAX";
        case TS_DOWNSAMPLE_SUM:   return "SUM";
        case TS_DOWNSAMPLE_FIRST: return "FIRST";
        case TS_DOWNSAMPLE_LAST:  return "LAST";
        case TS_DOWNSAMPLE_COUNT: return "COUNT";
        default:                  return "UNKNOWN";
    }
}

/** 执行降采样聚合 */
int32_t ts_retention_downsample(const ts_record_t *records,
                                 int32_t num_records,
                                 int64_t interval_ms,
                                 ts_downsampling_func_t func,
                                 ts_record_t *out_records,
                                 int32_t max_out) {
    if (!records || !out_records || num_records <= 0 || interval_ms <= 0) {
        return -1;
    }

    if (max_out <= 0) return 0;

    int32_t out_count = 0;
    int64_t bucket_start = -1;
    int64_t bucket_end = 0;
    double sum = 0;
    int32_t count = 0;
    double first_val = 0, last_val = 0;
    double min_val = 0, max_val = 0;
    bool first_in_bucket = true;

    for (int32_t i = 0; i < num_records; i++) {
        int64_t ts = records[i].timestamp;
        double val = records[i].value;

        /* 计算时间桶 */
        if (bucket_start < 0) {
            bucket_start = ts - (ts % interval_ms);
            bucket_end = bucket_start + interval_ms;
        }

        /* 检查是否需要开始新桶 */
        if (ts >= bucket_end) {
            if (out_count < max_out && count > 0) {
                double result_val = 0;

                switch (func) {
                    case TS_DOWNSAMPLE_AVG:
                        result_val = sum / count;
                        break;
                    case TS_DOWNSAMPLE_MIN:
                        result_val = min_val;
                        break;
                    case TS_DOWNSAMPLE_MAX:
                        result_val = max_val;
                        break;
                    case TS_DOWNSAMPLE_SUM:
                        result_val = sum;
                        break;
                    case TS_DOWNSAMPLE_FIRST:
                        result_val = first_val;
                        break;
                    case TS_DOWNSAMPLE_LAST:
                        result_val = last_val;
                        break;
                    case TS_DOWNSAMPLE_COUNT:
                        result_val = (double)count;
                        break;
                    default:
                        result_val = sum / count;
                        break;
                }

                out_records[out_count].timestamp = bucket_start;
                out_records[out_count].value = result_val;
                out_count++;
            }

            bucket_start = ts - (ts % interval_ms);
            bucket_end = bucket_start + interval_ms;
            count = 0;
            sum = 0;
            first_in_bucket = true;
        }

        count++;
        sum += val;
        last_val = val;

        if (first_in_bucket) {
            first_val = val;
            min_val = val;
            max_val = val;
            first_in_bucket = false;
        } else {
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
    }

    /* 输出最后一个桶 */
    if (out_count < max_out && count > 0) {
        double result_val = 0;

        switch (func) {
            case TS_DOWNSAMPLE_AVG:
                result_val = sum / count;
                break;
            case TS_DOWNSAMPLE_MIN:
                result_val = min_val;
                break;
            case TS_DOWNSAMPLE_MAX:
                result_val = max_val;
                break;
            case TS_DOWNSAMPLE_SUM:
                result_val = sum;
                break;
            case TS_DOWNSAMPLE_FIRST:
                result_val = first_val;
                break;
            case TS_DOWNSAMPLE_LAST:
                result_val = last_val;
                break;
            case TS_DOWNSAMPLE_COUNT:
                result_val = (double)count;
                break;
            default:
                result_val = sum / count;
                break;
        }

        out_records[out_count].timestamp = bucket_start;
        out_records[out_count].value = result_val;
        out_count++;
    }

    return out_count;
}

/* 占位函数在 ts_engine.c 中实现，保留工具函数 */
