/**
 * @file subscription.c
 * @brief 订阅管理器实现
 *
 * 实现订阅管理器的核心功能，支持多个订阅者订阅数据库变更。
 */
#include "db/subscription/subscription.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/**
 * @brief 变更队列节点
 */
typedef struct change_queue_node_s {
    cdc_change_t *change;
    struct change_queue_node_s *next;
} change_queue_node_t;

/**
 * @brief 变更队列
 */
typedef struct change_queue_s {
    change_queue_node_t *head;
    change_queue_node_t *tail;
    int32_t count;
    int32_t capacity;
} change_queue_t;

/* ========================================================================
 * 内部函数
 * ======================================================================== */

/**
 * @brief 创建变更队列
 */
static change_queue_t *change_queue_create(int32_t capacity) {
    change_queue_t *queue = (change_queue_t *)calloc(1, sizeof(change_queue_t));
    if (queue == NULL) return NULL;

    queue->capacity = capacity > 0 ? capacity : 1024;
    queue->head = queue->tail = NULL;
    queue->count = 0;

    return queue;
}

/**
 * @brief 销毁变更队列
 */
static void change_queue_destroy(change_queue_t *queue) {
    if (queue == NULL) return;

    change_queue_node_t *node = queue->head;
    while (node != NULL) {
        change_queue_node_t *next = node->next;
        if (node->change != NULL) {
            cdc_free_change(node->change);
        }
        free(node);
        node = next;
    }

    free(queue);
}

/**
 * @brief 入队
 */
static int change_queue_push(change_queue_t *queue, cdc_change_t *change) {
    if (queue == NULL || change == NULL) return -1;

    /* 检查容量 */
    if (queue->count >= queue->capacity) {
        LOG_WARN("变更队列已满，丢弃最旧的变更");
        change_queue_node_t *old = queue->head;
        queue->head = old->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
        }
        if (old->change != NULL) {
            cdc_free_change(old->change);
        }
        free(old);
        queue->count--;
    }

    change_queue_node_t *node = (change_queue_node_t *)malloc(sizeof(change_queue_node_t));
    if (node == NULL) return -1;

    node->change = change;
    node->next = NULL;

    if (queue->tail == NULL) {
        queue->head = queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }

    queue->count++;
    return 0;
}

/**
 * @brief 出队
 */
static cdc_change_t *change_queue_pop(change_queue_t *queue) {
    if (queue == NULL || queue->head == NULL) return NULL;

    change_queue_node_t *node = queue->head;
    cdc_change_t *change = node->change;

    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    free(node);
    queue->count--;

    return change;
}

/**
 * @brief 查找订阅
 */
static subscription_t *find_subscription(subscription_manager_t *mgr, const char *name) {
    if (mgr == NULL || name == NULL) return NULL;

    for (int32_t i = 0; i < mgr->subscription_count; i++) {
        if (mgr->subscriptions[i] != NULL &&
            strcmp(mgr->subscriptions[i]->name, name) == 0) {
            return mgr->subscriptions[i];
        }
    }

    return NULL;
}

/**
 * @brief 检查变更是否匹配订阅过滤条件
 */
static bool change_matches_filter(const subscription_t *sub, const cdc_change_t *change) {
    if (sub == NULL || change == NULL) return false;

    /* 检查关系 ID */
    if (sub->relation_count > 0) {
        bool found = false;
        for (int32_t i = 0; i < sub->relation_count; i++) {
            if (sub->relation_ids[i] == change->relation_id) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    /* 检查变更类型 */
    if (sub->change_type_count > 0) {
        bool found = false;
        for (int32_t i = 0; i < sub->change_type_count; i++) {
            if (sub->change_types[i] == change->type) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    /* 检查 LSN */
    if (change->lsn < sub->start_lsn) {
        return false;
    }

    return true;
}

/* ========================================================================
 * 订阅管理器 API 实现
 * ======================================================================== */

subscription_manager_t *subscription_manager_create(const char *data_dir) {
    if (data_dir == NULL) {
        LOG_ERROR("订阅管理器创建失败: data_dir 为空");
        return NULL;
    }

    subscription_manager_t *mgr = (subscription_manager_t *)calloc(1, sizeof(subscription_manager_t));
    if (mgr == NULL) {
        LOG_ERROR("订阅管理器分配失败");
        return NULL;
    }

    strncpy(mgr->data_dir, data_dir, sizeof(mgr->data_dir) - 1);
    mgr->subscription_count = 0;
    mgr->capacity = SUBSCRIPTION_MAX_COUNT;
    mgr->subscriptions = (subscription_t **)calloc(mgr->capacity, sizeof(subscription_t *));

    if (mgr->subscriptions == NULL) {
        free(mgr);
        return NULL;
    }

    /* 创建 CDC 上下文 */
    mgr->cdc = cdc_create(data_dir, 0);
    if (mgr->cdc == NULL) {
        free(mgr->subscriptions);
        free(mgr);
        return NULL;
    }

    /* 创建变更队列 */
    mgr->change_queue = change_queue_create(SUBSCRIPTION_BUFFER_SIZE);
    if (mgr->change_queue == NULL) {
        cdc_destroy(mgr->cdc);
        free(mgr->subscriptions);
        free(mgr);
        return NULL;
    }

    /* 创建数据目录 */
#ifdef _WIN32
    if (mkdir(data_dir) != 0 && errno != EEXIST) {
#else
    if (mkdir(data_dir, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_WARN("订阅数据目录创建失败: %s", data_dir);
    }

    LOG_INFO("订阅管理器创建成功: data_dir=%s", data_dir);
    return mgr;
}

void subscription_manager_destroy(subscription_manager_t *mgr) {
    if (mgr == NULL) return;

    /* 销毁所有订阅 */
    for (int32_t i = 0; i < mgr->subscription_count; i++) {
        if (mgr->subscriptions[i] != NULL) {
            subscription_destroy(mgr, mgr->subscriptions[i]->name);
        }
    }

    /* 销毁变更队列 */
    if (mgr->change_queue != NULL) {
        change_queue_destroy((change_queue_t *)mgr->change_queue);
    }

    /* 销毁 CDC 上下文 */
    if (mgr->cdc != NULL) {
        cdc_destroy(mgr->cdc);
    }

    free(mgr->subscriptions);
    free(mgr);

    LOG_INFO("订阅管理器已销毁");
}

/* ========================================================================
 * 订阅操作 API 实现
 * ======================================================================== */

subscription_t *subscription_create(subscription_manager_t *mgr,
                                    const char *name,
                                    subscription_callback_t callback,
                                    void *user_data) {
    if (mgr == NULL || name == NULL) {
        LOG_ERROR("订阅创建失败: 参数无效");
        return NULL;
    }

    /* 检查是否已存在同名订阅 */
    if (find_subscription(mgr, name) != NULL) {
        LOG_ERROR("订阅创建失败: 名称已存在: %s", name);
        return NULL;
    }

    /* 检查容量 */
    if (mgr->subscription_count >= mgr->capacity) {
        LOG_ERROR("订阅创建失败: 订阅数达到上限");
        return NULL;
    }

    /* 分配订阅结构 */
    subscription_t *sub = (subscription_t *)calloc(1, sizeof(subscription_t));
    if (sub == NULL) {
        LOG_ERROR("订阅分配失败");
        return NULL;
    }

    /* 初始化订阅 */
    strncpy(sub->name, name, SUBSCRIPTION_MAX_NAME - 1);
    sub->state = SUBSCRIPTION_STATE_INACTIVE;
    sub->callback = callback;
    sub->user_data = user_data;
    sub->start_lsn = cdc_get_current_lsn(mgr->cdc);
    sub->last_lsn = 0;
    sub->last_processed_lsn = 0;
    sub->total_changes = 0;
    sub->successful_deliveries = 0;
    sub->failed_deliveries = 0;

    /* 分配过滤数组 */
    sub->relation_ids = (int32_t *)malloc(SUBSCRIPTION_MAX_COUNT * sizeof(int32_t));
    sub->change_types = (cdc_change_type_t *)malloc(4 * sizeof(cdc_change_type_t));

    if (sub->relation_ids == NULL || sub->change_types == NULL) {
        free(sub->relation_ids);
        free(sub->change_types);
        free(sub);
        return NULL;
    }

    /* 默认订阅所有变更类型 */
    sub->change_types[sub->change_type_count++] = CDC_CHANGE_INSERT;
    sub->change_types[sub->change_type_count++] = CDC_CHANGE_UPDATE;
    sub->change_types[sub->change_type_count++] = CDC_CHANGE_DELETE;

    /* 添加到管理器 */
    mgr->subscriptions[mgr->subscription_count++] = sub;

    LOG_INFO("订阅创建成功: name=%s", name);
    return sub;
}

int subscription_destroy(subscription_manager_t *mgr, const char *name) {
    if (mgr == NULL || name == NULL) return -1;

    for (int32_t i = 0; i < mgr->subscription_count; i++) {
        if (mgr->subscriptions[i] != NULL &&
            strcmp(mgr->subscriptions[i]->name, name) == 0) {

            subscription_t *sub = mgr->subscriptions[i];

            /* 释放资源 */
            free(sub->relation_ids);
            free(sub->change_types);
            free(sub);

            /* 移动数组元素 */
            for (int32_t j = i; j < mgr->subscription_count - 1; j++) {
                mgr->subscriptions[j] = mgr->subscriptions[j + 1];
            }
            mgr->subscription_count--;

            LOG_INFO("订阅已销毁: name=%s", name);
            return 0;
        }
    }

    return -1;  /* 未找到 */
}

int subscription_activate(subscription_manager_t *mgr, const char *name) {
    subscription_t *sub = find_subscription(mgr, name);
    if (sub == NULL) {
        LOG_ERROR("订阅激活失败: 未找到订阅: %s", name);
        return -1;
    }

    sub->state = SUBSCRIPTION_STATE_ACTIVE;
    LOG_INFO("订阅已激活: name=%s", name);
    return 0;
}

int subscription_pause(subscription_manager_t *mgr, const char *name) {
    subscription_t *sub = find_subscription(mgr, name);
    if (sub == NULL) {
        LOG_ERROR("订阅暂停失败: 未找到订阅: %s", name);
        return -1;
    }

    sub->state = SUBSCRIPTION_STATE_PAUSED;
    LOG_INFO("订阅已暂停: name=%s", name);
    return 0;
}

int subscription_resume(subscription_manager_t *mgr, const char *name) {
    subscription_t *sub = find_subscription(mgr, name);
    if (sub == NULL) {
        LOG_ERROR("订阅恢复失败: 未找到订阅: %s", name);
        return -1;
    }

    sub->state = SUBSCRIPTION_STATE_ACTIVE;
    LOG_INFO("订阅已恢复: name=%s", name);
    return 0;
}

subscription_t *subscription_get(const subscription_manager_t *mgr, const char *name) {
    if (mgr == NULL || name == NULL) return NULL;
    return find_subscription((subscription_manager_t *)mgr, name);
}

/* ========================================================================
 * 过滤配置 API 实现
 * ======================================================================== */

int subscription_add_relation(subscription_t *sub, int32_t relation_id) {
    if (sub == NULL) return -1;

    /* 检查是否已存在 */
    for (int32_t i = 0; i < sub->relation_count; i++) {
        if (sub->relation_ids[i] == relation_id) {
            return 0;
        }
    }

    sub->relation_ids[sub->relation_count++] = relation_id;
    LOG_DEBUG("订阅添加关系: name=%s, relation_id=%d", sub->name, relation_id);
    return 0;
}

int subscription_remove_relation(subscription_t *sub, int32_t relation_id) {
    if (sub == NULL) return -1;

    for (int32_t i = 0; i < sub->relation_count; i++) {
        if (sub->relation_ids[i] == relation_id) {
            for (int32_t j = i; j < sub->relation_count - 1; j++) {
                sub->relation_ids[j] = sub->relation_ids[j + 1];
            }
            sub->relation_count--;
            return 0;
        }
    }

    return -1;
}

int subscription_add_change_type(subscription_t *sub, cdc_change_type_t type) {
    if (sub == NULL) return -1;

    /* 检查是否已存在 */
    for (int32_t i = 0; i < sub->change_type_count; i++) {
        if (sub->change_types[i] == type) {
            return 0;
        }
    }

    if (sub->change_type_count >= 4) {
        return -1;
    }

    sub->change_types[sub->change_type_count++] = type;
    LOG_DEBUG("订阅添加变更类型: name=%s, type=%d", sub->name, type);
    return 0;
}

void subscription_set_start_lsn(subscription_t *sub, uint64_t lsn) {
    if (sub == NULL) return;
    sub->start_lsn = lsn;
}

/* ========================================================================
 * 变更分发 API 实现
 * ======================================================================== */

int subscription_process_change(subscription_manager_t *mgr, const cdc_change_t *change) {
    if (mgr == NULL || change == NULL) return -1;

    /* 将变更加入队列 */
    cdc_change_t *copy = (cdc_change_t *)malloc(sizeof(cdc_change_t));
    if (copy == NULL) return -1;

    memcpy(copy, change, sizeof(cdc_change_t));

    /* 复制元组数据 */
    if (change->old_tuple != NULL && change->old_tuple_size > 0) {
        copy->old_tuple = malloc((size_t)change->old_tuple_size);
        if (copy->old_tuple != NULL) {
            memcpy(copy->old_tuple, change->old_tuple, (size_t)change->old_tuple_size);
        }
    }
    if (change->new_tuple != NULL && change->new_tuple_size > 0) {
        copy->new_tuple = malloc((size_t)change->new_tuple_size);
        if (copy->new_tuple != NULL) {
            memcpy(copy->new_tuple, change->new_tuple, (size_t)change->new_tuple_size);
        }
    }
    copy->next = NULL;

    change_queue_push((change_queue_t *)mgr->change_queue, copy);
    return 0;
}

int subscription_notify_all(subscription_manager_t *mgr, const cdc_change_t *change) {
    if (mgr == NULL || change == NULL) return 0;

    int notified = 0;

    /* 遍历所有订阅 */
    for (int32_t i = 0; i < mgr->subscription_count; i++) {
        subscription_t *sub = mgr->subscriptions[i];
        if (sub == NULL) continue;

        /* 检查订阅状态 */
        if (sub->state != SUBSCRIPTION_STATE_ACTIVE) {
            continue;
        }

        /* 检查过滤条件 */
        if (!change_matches_filter(sub, change)) {
            continue;
        }

        /* 调用回调 */
        sub->total_changes++;

        if (sub->callback != NULL) {
            int ret = sub->callback(change, sub->user_data);
            if (ret == 0) {
                sub->successful_deliveries++;
                notified++;
            } else {
                sub->failed_deliveries++;
                LOG_WARN("订阅投递失败: name=%s, ret=%d", sub->name, ret);
            }
        } else {
            sub->successful_deliveries++;
            notified++;
        }

        /* 更新 LSN */
        if (change->lsn > sub->last_processed_lsn) {
            sub->last_processed_lsn = change->lsn;
        }
    }

    return notified;
}

void subscription_ack(subscription_manager_t *mgr, uint64_t lsn) {
    if (mgr == NULL) return;

    for (int32_t i = 0; i < mgr->subscription_count; i++) {
        subscription_t *sub = mgr->subscriptions[i];
        if (sub != NULL && sub->last_processed_lsn <= lsn) {
            sub->last_lsn = lsn;
        }
    }
}

/* ========================================================================
 * 统计 API 实现
 * ======================================================================== */

void subscription_get_stats(const subscription_t *sub,
                            uint64_t *out_total,
                            uint64_t *out_success,
                            uint64_t *out_failed) {
    if (sub == NULL) return;

    if (out_total) *out_total = sub->total_changes;
    if (out_success) *out_success = sub->successful_deliveries;
    if (out_failed) *out_failed = sub->failed_deliveries;
}

void subscription_reset_stats(subscription_t *sub) {
    if (sub == NULL) return;

    sub->total_changes = 0;
    sub->successful_deliveries = 0;
    sub->failed_deliveries = 0;
}
