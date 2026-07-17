/**
 * @file aging.c
 * @brief 数据老化实现
 *
 * 实现数据老化模块的核心功能，支持冷热数据分层、自动清理和归档。
 */
#include "db/aging/aging.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/**
 * @brief 数据热度记录
 */
typedef struct temp_record_s {
    void *data_id;
    uint64_t last_access_time;
    uint64_t create_time;
    int32_t access_count;
    int32_t current_tier;
    int64_t data_size;
    struct temp_record_s *next;
} temp_record_t;

/**
 * @brief 热度记录管理器
 */
typedef struct temp_manager_s {
    temp_record_t **records;
    int32_t count;
    int32_t capacity;
} temp_manager_t;

/* ========================================================================
 * 内部函数
 * ======================================================================== */

/**
 * @brief 计算数据热度
 */
static float calculate_temperature(const data_temperature_t *temp,
                                   const aging_policy_config_t *policies,
                                   int32_t policy_count) {
    if (temp == NULL) return 0.0f;

    float score = 1.0f;
    uint64_t now = (uint64_t)time(NULL) * 1000000;  /* 微秒 */
    uint64_t age_seconds = (now - temp->create_time) / 1000000;
    uint64_t idle_seconds = (now - temp->last_access_time) / 1000000;

    for (int32_t i = 0; i < policy_count; i++) {
        const aging_policy_config_t *p = &policies[i];

        switch (p->type) {
            case AGING_POLICY_TTL:
                /* TTL 策略：年龄越大，热度越低 */
                if (p->config.ttl.max_age_seconds > 0) {
                    float age_factor = 1.0f - (float)age_seconds / (float)p->config.ttl.max_age_seconds;
                    if (age_factor < 0) age_factor = 0;
                    score = score * 0.5f + age_factor * 0.5f;
                }
                break;

            case AGING_POLICY_ACCESS_FREQ:
                /* 访问频率策略：访问次数越多，热度越高 */
                if (p->config.access_freq.max_access_count > 0) {
                    float freq_factor = (float)temp->access_count /
                                        (float)p->config.access_freq.max_access_count;
                    if (freq_factor > 1.0f) freq_factor = 1.0f;
                    score = score * 0.7f + freq_factor * 0.3f;
                }
                break;

            case AGING_POLICY_LRU:
                /* LRU 策略：空闲时间越长，热度越低 */
                if (idle_seconds > 0) {
                    float idle_factor = 1.0f / (1.0f + logf((float)idle_seconds + 1.0f));
                    score = score * 0.6f + idle_factor * 0.4f;
                }
                break;

            default:
                break;
        }
    }

    return score;
}

/**
 * @brief 推荐层级
 */
static int32_t recommend_tier(float temperature) {
    if (temperature >= 0.7f) return AGING_TIER_HOT;
    if (temperature >= 0.3f) return AGING_TIER_WARM;
    return AGING_TIER_COLD;
}

/**
 * @brief 推荐动作
 */
static aging_action_t recommend_action(float temperature, int32_t current_tier) {
    int32_t target_tier = recommend_tier(temperature);

    if (target_tier == current_tier) {
        return AGING_ACTION_KEEP;
    }

    if (target_tier < current_tier) {
        /* 变热，可能需要移至热层 */
        return AGING_ACTION_MOVE_HOT;
    }

    if (temperature < 0.1f) {
        return AGING_ACTION_DELETE;
    }

    if (temperature < 0.2f) {
        return AGING_ACTION_ARCHIVE;
    }

    if (target_tier == AGING_TIER_WARM) {
        return AGING_ACTION_MOVE_WARM;
    }

    return AGING_ACTION_MOVE_COLD;
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

aging_manager_t *aging_manager_create(const char *data_dir) {
    if (data_dir == NULL) {
        LOG_ERROR("老化管理器创建失败: data_dir 为空");
        return NULL;
    }

    aging_manager_t *mgr = (aging_manager_t *)calloc(1, sizeof(aging_manager_t));
    if (mgr == NULL) {
        LOG_ERROR("老化管理器分配失败");
        return NULL;
    }

    strncpy(mgr->data_dir, data_dir, sizeof(mgr->data_dir) - 1);
    mgr->policy_count = 0;
    mgr->capacity = 16;
    mgr->schedule_interval = AGING_DEFAULT_SCHEDULE_INTERVAL;

    /* 分配策略数组 */
    mgr->policies = (aging_policy_config_t *)calloc(mgr->capacity,
                                                     sizeof(aging_policy_config_t));
    if (mgr->policies == NULL) {
        free(mgr);
        return NULL;
    }

    /* 设置默认容量（1GB 热层，10GB 温层，100GB 冷层） */
    mgr->hot_capacity = 1024LL * 1024 * 1024;              /* 1GB */
    mgr->warm_capacity = 10LL * 1024 * 1024 * 1024;        /* 10GB */
    mgr->cold_capacity = 100LL * 1024 * 1024 * 1024;       /* 100GB */

    /* 创建数据目录 */
#ifdef _WIN32
    if (mkdir(data_dir) != 0 && errno != EEXIST) {
#else
    if (mkdir(data_dir, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_WARN("老化数据目录创建失败: %s", data_dir);
    }

    LOG_INFO("老化管理器创建成功: schedule_interval=%us", mgr->schedule_interval);
    return mgr;
}

void aging_manager_destroy(aging_manager_t *mgr) {
    if (mgr == NULL) return;

    aging_save_state(mgr);
    free(mgr->policies);
    free(mgr);
    LOG_INFO("老化管理器已销毁");
}

int aging_load_state(aging_manager_t *mgr) {
    if (mgr == NULL) return -1;

    char path[512];
    snprintf(path, sizeof(path), "%s/aging.state", mgr->data_dir);

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        LOG_INFO("老化状态文件不存在");
        return 0;
    }

    /* 验证魔数 */
    uint32_t magic;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != AGING_FILE_MAGIC) {
        LOG_WARN("老化状态文件魔数无效");
        fclose(fp);
        return -1;
    }

    /* 验证版本 */
    uint32_t version;
    if (fread(&version, sizeof(version), 1, fp) != 1 || version != AGING_FILE_VERSION) {
        LOG_WARN("老化状态文件版本不匹配");
        fclose(fp);
        return -1;
    }

    /* 读取容量 */
    fread(&mgr->hot_capacity, sizeof(mgr->hot_capacity), 1, fp);
    fread(&mgr->warm_capacity, sizeof(mgr->warm_capacity), 1, fp);
    fread(&mgr->cold_capacity, sizeof(mgr->cold_capacity), 1, fp);

    /* 读取使用量 */
    fread(&mgr->hot_usage, sizeof(mgr->hot_usage), 1, fp);
    fread(&mgr->warm_usage, sizeof(mgr->warm_usage), 1, fp);
    fread(&mgr->cold_usage, sizeof(mgr->cold_usage), 1, fp);

    /* 读取策略 */
    fread(&mgr->policy_count, sizeof(mgr->policy_count), 1, fp);
    for (int32_t i = 0; i < mgr->policy_count && i < mgr->capacity; i++) {
        fread(&mgr->policies[i], sizeof(aging_policy_config_t), 1, fp);
    }

    /* 读取统计 */
    fread(&mgr->stats, sizeof(aging_stats_t), 1, fp);

    fclose(fp);
    LOG_INFO("老化状态已加载");
    return 0;
}

int aging_save_state(aging_manager_t *mgr) {
    if (mgr == NULL) return -1;

    char path[512];
    snprintf(path, sizeof(path), "%s/aging.state", mgr->data_dir);

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        LOG_ERROR("老化状态文件创建失败: %s", path);
        return -1;
    }

    /* 写入魔数 */
    uint32_t magic = AGING_FILE_MAGIC;
    fwrite(&magic, sizeof(magic), 1, fp);

    /* 写入版本 */
    uint32_t version = AGING_FILE_VERSION;
    fwrite(&version, sizeof(version), 1, fp);

    /* 写入容量 */
    fwrite(&mgr->hot_capacity, sizeof(mgr->hot_capacity), 1, fp);
    fwrite(&mgr->warm_capacity, sizeof(mgr->warm_capacity), 1, fp);
    fwrite(&mgr->cold_capacity, sizeof(mgr->cold_capacity), 1, fp);

    /* 写入使用量 */
    fwrite(&mgr->hot_usage, sizeof(mgr->hot_usage), 1, fp);
    fwrite(&mgr->warm_usage, sizeof(mgr->warm_usage), 1, fp);
    fwrite(&mgr->cold_usage, sizeof(mgr->cold_usage), 1, fp);

    /* 写入策略 */
    fwrite(&mgr->policy_count, sizeof(mgr->policy_count), 1, fp);
    for (int32_t i = 0; i < mgr->policy_count; i++) {
        fwrite(&mgr->policies[i], sizeof(aging_policy_config_t), 1, fp);
    }

    /* 写入统计 */
    fwrite(&mgr->stats, sizeof(aging_stats_t), 1, fp);

    fclose(fp);
    LOG_INFO("老化状态已保存");
    return 0;
}

/* ========================================================================
 * 策略管理 API 实现
 * ======================================================================== */

int aging_add_policy(aging_manager_t *mgr, const aging_policy_config_t *policy) {
    if (mgr == NULL || policy == NULL) return -1;

    /* 检查是否已存在同类型策略 */
    for (int32_t i = 0; i < mgr->policy_count; i++) {
        if (mgr->policies[i].type == policy->type) {
            /* 更新现有策略 */
            mgr->policies[i] = *policy;
            LOG_INFO("老化策略已更新: type=%d", policy->type);
            return 0;
        }
    }

    /* 检查容量 */
    if (mgr->policy_count >= mgr->capacity) {
        int32_t new_capacity = mgr->capacity * 2;
        aging_policy_config_t *new_policies = (aging_policy_config_t *)realloc(
            mgr->policies, (size_t)new_capacity * sizeof(aging_policy_config_t));
        if (new_policies == NULL) {
            LOG_ERROR("老化策略数组扩展失败");
            return -1;
        }
        mgr->policies = new_policies;
        mgr->capacity = new_capacity;
    }

    mgr->policies[mgr->policy_count++] = *policy;
    LOG_INFO("老化策略已添加: type=%d, priority=%d", policy->type, policy->priority);
    return 0;
}

int aging_remove_policy(aging_manager_t *mgr, aging_policy_type_t policy_type) {
    if (mgr == NULL) return -1;

    for (int32_t i = 0; i < mgr->policy_count; i++) {
        if (mgr->policies[i].type == policy_type) {
            /* 移动数组元素 */
            for (int32_t j = i; j < mgr->policy_count - 1; j++) {
                mgr->policies[j] = mgr->policies[j + 1];
            }
            mgr->policy_count--;
            LOG_INFO("老化策略已移除: type=%d", policy_type);
            return 0;
        }
    }

    return -1;  /* 未找到 */
}

const aging_policy_config_t *aging_get_policy(const aging_manager_t *mgr,
                                               aging_policy_type_t policy_type) {
    if (mgr == NULL) return NULL;

    for (int32_t i = 0; i < mgr->policy_count; i++) {
        if (mgr->policies[i].type == policy_type) {
            return &mgr->policies[i];
        }
    }

    return NULL;
}

/* ========================================================================
 * 容量管理 API 实现
 * ======================================================================== */

void aging_set_capacity(aging_manager_t *mgr,
                        int64_t hot_capacity,
                        int64_t warm_capacity,
                        int64_t cold_capacity) {
    if (mgr == NULL) return;

    mgr->hot_capacity = hot_capacity;
    mgr->warm_capacity = warm_capacity;
    mgr->cold_capacity = cold_capacity;

    LOG_INFO("老化容量已设置: hot=%ld, warm=%ld, cold=%ld",
             hot_capacity, warm_capacity, cold_capacity);
}

bool aging_needs_eviction(const aging_manager_t *mgr, int32_t tier, int64_t data_size) {
    if (mgr == NULL) return false;

    int64_t capacity, usage;
    switch (tier) {
        case AGING_TIER_HOT:
            capacity = mgr->hot_capacity;
            usage = mgr->hot_usage;
            break;
        case AGING_TIER_WARM:
            capacity = mgr->warm_capacity;
            usage = mgr->warm_usage;
            break;
        case AGING_TIER_COLD:
            capacity = mgr->cold_capacity;
            usage = mgr->cold_usage;
            break;
        default:
            return false;
    }

    return (usage + data_size) > capacity;
}

/* ========================================================================
 * 热度评估 API 实现
 * ======================================================================== */

void aging_record_access(aging_manager_t *mgr, void *data_id, int64_t data_size) {
    /* TODO: 实现热度记录存储和更新 */
    (void)mgr;
    (void)data_id;
    (void)data_size;
}

int32_t aging_evaluate_tier(const aging_manager_t *mgr, const data_temperature_t *temp) {
    if (mgr == NULL || temp == NULL) return AGING_TIER_COLD;

    float temperature = calculate_temperature(temp, mgr->policies, mgr->policy_count);
    return recommend_tier(temperature);
}

aging_action_t aging_recommend_action(const aging_manager_t *mgr,
                                       const data_temperature_t *temp) {
    if (mgr == NULL || temp == NULL) return AGING_ACTION_KEEP;

    float temperature = calculate_temperature(temp, mgr->policies, mgr->policy_count);
    return recommend_action(temperature, temp->current_tier);
}

/* ========================================================================
 * 执行老化 API 实现
 * ======================================================================== */

int32_t aging_schedule(aging_manager_t *mgr,
                       aging_action_t *out_actions,
                       int32_t max_actions) {
    if (mgr == NULL || out_actions == NULL || max_actions <= 0) return 0;

    /* TODO: 实现调度逻辑，扫描所有数据并生成动作列表 */
    /* 目前返回 0（无动作） */
    (void)mgr;
    (void)out_actions;
    return 0;
}

int aging_evict(aging_manager_t *mgr,
                void *data_id,
                aging_callback_t callback,
                void *user_data) {
    if (mgr == NULL || callback == NULL) return -1;

    int ret = callback(data_id, AGING_ACTION_DELETE, user_data);
    if (ret == 0) {
        mgr->stats.total_evictions++;
    }

    LOG_DEBUG("老化淘汰执行: data_id=%p", data_id);
    return ret;
}

int aging_archive(aging_manager_t *mgr,
                  void *data_id,
                  aging_callback_t callback,
                  void *user_data) {
    if (mgr == NULL || callback == NULL) return -1;

    int ret = callback(data_id, AGING_ACTION_ARCHIVE, user_data);
    if (ret == 0) {
        mgr->stats.total_archives++;
    }

    LOG_DEBUG("老化归档执行: data_id=%p", data_id);
    return ret;
}

int aging_delete(aging_manager_t *mgr,
                 void *data_id,
                 aging_callback_t callback,
                 void *user_data) {
    if (mgr == NULL || callback == NULL) return -1;

    int ret = callback(data_id, AGING_ACTION_DELETE, user_data);
    if (ret == 0) {
        mgr->stats.total_deletes++;
    }

    LOG_DEBUG("老化删除执行: data_id=%p", data_id);
    return ret;
}

/* ========================================================================
 * 统计 API 实现
 * ======================================================================== */

void aging_get_stats(const aging_manager_t *mgr, aging_stats_t *stats) {
    if (mgr == NULL || stats == NULL) return;

    *stats = mgr->stats;
}

void aging_reset_stats(aging_manager_t *mgr) {
    if (mgr == NULL) return;

    memset(&mgr->stats, 0, sizeof(aging_stats_t));
}
