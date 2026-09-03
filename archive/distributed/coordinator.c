/**
 * @file coordinator.c
 * @brief 多节点协调核心实现
 */

#include "db/core/coordinator.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

/* 获取当前时间戳（毫秒） */
static uint64_t get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
}

/* ========================================================================
 * 节点注册与发现
 * ======================================================================== */

NodeRegistry *node_registry_create(const char *cluster_id) {
    NodeRegistry *registry = (NodeRegistry *)calloc(1, sizeof(NodeRegistry));
    if (!registry) return NULL;

    if (cluster_id) {
        snprintf(registry->cluster_id, sizeof(registry->cluster_id), "%s", cluster_id);
    }

    registry->capacity = 16;
    registry->nodes = (NodeRegistration *)calloc(registry->capacity, sizeof(NodeRegistration));

    pthread_mutex_init(&registry->lock, NULL);

    return registry;
}

void node_registry_destroy(NodeRegistry *registry) {
    if (!registry) return;

    pthread_mutex_destroy(&registry->lock);
    free(registry->nodes);
    free(registry);
}

int node_registry_register(NodeRegistry *registry, const NodeMetadata *metadata) {
    if (!registry || !metadata) return -1;

    pthread_mutex_lock(&registry->lock);

    /* 检查是否已存在 */
    for (size_t i = 0; i < registry->num_nodes; i++) {
        if (strcmp(registry->nodes[i].metadata.node_id, metadata->node_id) == 0) {
            /* 更新现有节点 */
            registry->nodes[i].metadata = *metadata;
            registry->nodes[i].health = NODE_HEALTH_HEALTHY;
            registry->nodes[i].last_seen = get_current_time_ms();
            registry->cluster_version++;
            pthread_mutex_unlock(&registry->lock);
            return 0;
        }
    }

    /* 添加新节点 */
    if (registry->num_nodes >= registry->capacity) {
        registry->capacity *= 2;
        registry->nodes = (NodeRegistration *)realloc(registry->nodes,
                                                      registry->capacity * sizeof(NodeRegistration));
    }

    NodeRegistration *reg = &registry->nodes[registry->num_nodes++];
    reg->metadata = *metadata;
    reg->registered_at = get_current_time_ms();
    reg->last_seen = get_current_time_ms();
    reg->health = NODE_HEALTH_HEALTHY;
    reg->is_active = true;
    reg->lease_ttl = 30;  /* 默认 30 秒 */

    registry->cluster_version++;

    pthread_mutex_unlock(&registry->lock);
    return 0;
}

int node_registry_unregister(NodeRegistry *registry, const char *node_id) {
    if (!registry || !node_id) return -1;

    pthread_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->num_nodes; i++) {
        if (strcmp(registry->nodes[i].metadata.node_id, node_id) == 0) {
            /* 移动后面的节点 */
            for (size_t j = i; j < registry->num_nodes - 1; j++) {
                registry->nodes[j] = registry->nodes[j + 1];
            }
            registry->num_nodes--;
            registry->cluster_version++;
            pthread_mutex_unlock(&registry->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&registry->lock);
    return -1;
}

int node_registry_heartbeat(NodeRegistry *registry, const char *node_id) {
    if (!registry || !node_id) return -1;

    pthread_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->num_nodes; i++) {
        if (strcmp(registry->nodes[i].metadata.node_id, node_id) == 0) {
            registry->nodes[i].last_seen = get_current_time_ms();
            registry->nodes[i].health = NODE_HEALTH_HEALTHY;
            registry->nodes[i].is_active = true;
            pthread_mutex_unlock(&registry->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&registry->lock);
    return -1;
}

const NodeRegistration *node_registry_get(NodeRegistry *registry, const char *node_id) {
    if (!registry || !node_id) return NULL;

    pthread_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->num_nodes; i++) {
        if (strcmp(registry->nodes[i].metadata.node_id, node_id) == 0) {
            pthread_mutex_unlock(&registry->lock);
            return &registry->nodes[i];
        }
    }

    pthread_mutex_unlock(&registry->lock);
    return NULL;
}

const NodeRegistration **node_registry_get_active(NodeRegistry *registry, size_t *out_count) {
    if (!registry || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    pthread_mutex_lock(&registry->lock);

    static const NodeRegistration *results[256];
    size_t count = 0;
    uint64_t now = get_current_time_ms();

    for (size_t i = 0; i < registry->num_nodes && count < 256; i++) {
        /* 检查心跳是否过期（超过 2 个租约周期） */
        uint64_t elapsed = now - registry->nodes[i].last_seen;
        if (elapsed < registry->nodes[i].lease_ttl * 2000) {
            results[count++] = &registry->nodes[i];
        }
    }

    pthread_mutex_unlock(&registry->lock);

    *out_count = count;
    return results;
}

int node_registry_update_load(NodeRegistry *registry,
                             const char *node_id,
                             const NodeMetadata *load_info) {
    if (!registry || !node_id || !load_info) return -1;

    pthread_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->num_nodes; i++) {
        if (strcmp(registry->nodes[i].metadata.node_id, node_id) == 0) {
            NodeRegistration *reg = &registry->nodes[i];
            reg->metadata.cpu_usage = load_info->cpu_usage;
            reg->metadata.memory_usage = load_info->memory_usage;
            reg->metadata.disk_usage = load_info->disk_usage;
            reg->metadata.load_avg = load_info->load_avg;
            reg->metadata.num_connections = load_info->num_connections;
            pthread_mutex_unlock(&registry->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&registry->lock);
    return -1;
}

int node_registry_cleanup(NodeRegistry *registry) {
    if (!registry) return -1;

    pthread_mutex_lock(&registry->lock);

    uint64_t now = get_current_time_ms();
    size_t removed = 0;

    for (size_t i = 0; i < registry->num_nodes; i++) {
        uint64_t elapsed = now - registry->nodes[i].last_seen;
        if (elapsed > registry->nodes[i].lease_ttl * 1000 * 3) {
            /* 心跳过期，标记为不活跃 */
            registry->nodes[i].is_active = false;
            registry->nodes[i].health = NODE_HEALTH_UNHEALTHY;
            removed++;
        }
    }

    if (removed > 0) {
        registry->cluster_version++;
    }

    pthread_mutex_unlock(&registry->lock);
    return 0;
}

/* ========================================================================
 * 全局锁服务
 * ======================================================================== */

GlobalLockService *global_lock_service_create(const char *service_id, RaftServer *raft) {
    GlobalLockService *service = (GlobalLockService *)calloc(1, sizeof(GlobalLockService));
    if (!service) return NULL;

    if (service_id) {
        snprintf(service->service_id, sizeof(service->service_id), "%s", service_id);
    }

    service->raft = raft;

    service->locks_capacity = 64;
    service->locks = (LockInfo **)calloc(service->locks_capacity, sizeof(LockInfo *));

    service->default_ttl_ms = 30000;    /* 30 秒 */
    service->max_lock_wait_ms = 10000;   /* 10 秒 */

    pthread_mutex_init(&service->lock, NULL);

    return service;
}

void global_lock_service_destroy(GlobalLockService *service) {
    if (!service) return;

    pthread_mutex_destroy(&service->lock);

    for (size_t i = 0; i < service->num_locks; i++) {
        free(service->locks[i]->holders);
        free(service->locks[i]);
    }
    free(service->locks);

    free(service);
}

static LockInfo *find_or_create_lock(GlobalLockService *service, const char *lock_key) {
    if (!service || !lock_key) return NULL;

    for (size_t i = 0; i < service->num_locks; i++) {
        if (strcmp(service->locks[i]->lock_key, lock_key) == 0) {
            return service->locks[i];
        }
    }

    /* 创建新锁 */
    if (service->num_locks >= service->locks_capacity) {
        service->locks_capacity *= 2;
        service->locks = (LockInfo **)realloc(service->locks,
                                              service->locks_capacity * sizeof(LockInfo *));
    }

    LockInfo *lock = (LockInfo *)calloc(1, sizeof(LockInfo));
    if (!lock) return NULL;

    snprintf(lock->lock_key, sizeof(lock->lock_key), "%s", lock_key);
    lock->created_at = get_current_time_ms();
    lock->holders = (LockHolder *)calloc(4, sizeof(LockHolder));
    lock->num_holders = 0;

    service->locks[service->num_locks++] = lock;
    return lock;
}

int global_lock_acquire(GlobalLockService *service,
                        const char *lock_key,
                        const char *holder_id,
                        uint64_t txn_id,
                        LockMode mode,
                        uint64_t timeout_ms) {
    if (!service || !lock_key || !holder_id) return -1;

    pthread_mutex_lock(&service->lock);

    LockInfo *lock = find_or_create_lock(service, lock_key);
    if (!lock) {
        pthread_mutex_unlock(&service->lock);
        return -1;
    }

    /* 检查是否可获取 */
    if (lock->num_holders > 0) {
        /* 检查是否可共享 */
        if (mode == LOCK_MODE_SHARED && lock->mode == LOCK_MODE_SHARED) {
            /* 共享锁可以多个持有者 */
        } else {
            /* 排他锁或已有排他锁，等待 */
            pthread_mutex_unlock(&service->lock);
            /* 简化：直接失败 */
            return -1;
        }
    }

    /* 获取锁 */
    lock->mode = mode;
    lock->holders[lock->num_holders].txn_id = txn_id;
    snprintf(lock->holders[lock->num_holders].holder_id, sizeof(lock->holders[0].holder_id), "%s", holder_id);
    lock->holders[lock->num_holders].acquired_at = get_current_time_ms();
    lock->holders[lock->num_holders].expires_at = lock->holders[lock->num_holders].acquired_at + service->default_ttl_ms;
    lock->holders[lock->num_holders].mode = mode;
    lock->num_holders++;
    lock->num_acquires++;

    service->total_acquires++;

    pthread_mutex_unlock(&service->lock);
    return 0;
}

bool global_lock_try_acquire(GlobalLockService *service,
                             const char *lock_key,
                             const char *holder_id,
                             uint64_t txn_id,
                             LockMode mode) {
    return global_lock_acquire(service, lock_key, holder_id, txn_id, mode, 0) == 0;
}

int global_lock_release(GlobalLockService *service,
                        const char *lock_key,
                        const char *holder_id) {
    if (!service || !lock_key || !holder_id) return -1;

    pthread_mutex_lock(&service->lock);

    for (size_t i = 0; i < service->num_locks; i++) {
        LockInfo *lock = service->locks[i];
        if (strcmp(lock->lock_key, lock_key) == 0) {
            for (size_t j = 0; j < lock->num_holders; j++) {
                if (strcmp(lock->holders[j].holder_id, holder_id) == 0) {
                    /* 移除持有者 */
                    for (size_t k = j; k < lock->num_holders - 1; k++) {
                        lock->holders[k] = lock->holders[k + 1];
                    }
                    lock->num_holders--;
                    service->total_releases++;

                    /* 如果没有持有者了，清理锁 */
                    if (lock->num_holders == 0) {
                        lock->mode = LOCK_MODE_SHARED;  /* 重置 */
                    }

                    pthread_mutex_unlock(&service->lock);
                    return 0;
                }
            }
        }
    }

    pthread_mutex_unlock(&service->lock);
    return -1;
}

int global_lock_renew(GlobalLockService *service,
                      const char *lock_key,
                      const char *holder_id,
                      uint64_t new_ttl_ms) {
    if (!service || !lock_key || !holder_id) return -1;

    pthread_mutex_lock(&service->lock);

    for (size_t i = 0; i < service->num_locks; i++) {
        LockInfo *lock = service->locks[i];
        if (strcmp(lock->lock_key, lock_key) == 0) {
            for (size_t j = 0; j < lock->num_holders; j++) {
                if (strcmp(lock->holders[j].holder_id, holder_id) == 0) {
                    lock->holders[j].expires_at = get_current_time_ms() + new_ttl_ms;
                    pthread_mutex_unlock(&service->lock);
                    return 0;
                }
            }
        }
    }

    pthread_mutex_unlock(&service->lock);
    return -1;
}

const LockInfo *global_lock_get_info(GlobalLockService *service, const char *lock_key) {
    if (!service || !lock_key) return NULL;

    pthread_mutex_lock(&service->lock);

    for (size_t i = 0; i < service->num_locks; i++) {
        if (strcmp(service->locks[i]->lock_key, lock_key) == 0) {
            pthread_mutex_unlock(&service->lock);
            return service->locks[i];
        }
    }

    pthread_mutex_unlock(&service->lock);
    return NULL;
}

bool global_lock_is_held(GlobalLockService *service, const char *lock_key) {
    const LockInfo *info = global_lock_get_info(service, lock_key);
    return info && info->num_holders > 0;
}

LockMode global_lock_get_mode(GlobalLockService *service, const char *lock_key) {
    const LockInfo *info = global_lock_get_info(service, lock_key);
    return info ? info->mode : LOCK_MODE_SHARED;
}

const LockInfo **global_lock_list(GlobalLockService *service, size_t *out_count) {
    if (!service || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    pthread_mutex_lock(&service->lock);
    *out_count = service->num_locks;
    pthread_mutex_unlock(&service->lock);

    return (const LockInfo **)service->locks;
}

int global_lock_cleanup_expired(GlobalLockService *service) {
    if (!service) return -1;

    pthread_mutex_lock(&service->lock);

    uint64_t now = get_current_time_ms();
    size_t cleaned = 0;

    for (size_t i = 0; i < service->num_locks; i++) {
        LockInfo *lock = service->locks[i];
        for (int j = (int)lock->num_holders - 1; j >= 0; j--) {
            if (lock->holders[j].expires_at < now) {
                /* 移除过期持有者 */
                for (size_t k = (size_t)j; k < lock->num_holders - 1; k++) {
                    lock->holders[k] = lock->holders[k + 1];
                }
                lock->num_holders--;
                cleaned++;
            }
        }
    }

    pthread_mutex_unlock(&service->lock);
    return (int)cleaned;
}

/* ========================================================================
 * 领导者选举
 * ======================================================================== */

ElectionResult *execute_election(NodeRegistry *registry,
                                 const ElectionConfig *config,
                                 RaftServer *raft) {
    if (!registry || !config) return NULL;

    ElectionResult *result = (ElectionResult *)calloc(1, sizeof(ElectionResult));
    if (!result) return NULL;

    uint64_t start_time = get_current_time_ms();

    /* 简化实现：基于最低 ID 选择 Leader */
    const char *best_node = NULL;

    pthread_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->num_nodes; i++) {
        if (!registry->nodes[i].is_active) continue;
        if (registry->nodes[i].metadata.role != COORD_ROLE_MEMBER) continue;

        if (!best_node || strcmp(registry->nodes[i].metadata.node_id, best_node) < 0) {
            best_node = registry->nodes[i].metadata.node_id;
        }
    }

    pthread_mutex_unlock(&registry->lock);

    if (best_node) {
        result->elected = true;
        snprintf(result->leader_id, sizeof(result->leader_id), "%s", best_node);
        result->term = raft ? raft->current_term + 1 : 1;
        result->votes_received = 1;
    } else {
        result->elected = false;
    }

    result->election_time_ms = get_current_time_ms() - start_time;

    return result;
}

bool is_leader_valid(NodeRegistry *registry, const char *leader_id) {
    if (!registry || !leader_id) return false;

    const NodeRegistration *node = node_registry_get(registry, leader_id);
    return node && node->is_active && node->health == NODE_HEALTH_HEALTHY;
}

int transfer_leadership(NodeRegistry *registry,
                       const char *from_id,
                       const char *to_id) {
    (void)registry; (void)from_id; (void)to_id;
    /* 简化实现 */
    return 0;
}

const NodeRegistration *get_current_leader(NodeRegistry *registry) {
    if (!registry) return NULL;

    pthread_mutex_lock(&registry->lock);

    for (size_t i = 0; i < registry->num_nodes; i++) {
        if (registry->nodes[i].metadata.role == COORD_ROLE_COORDINATOR &&
            registry->nodes[i].is_active) {
            pthread_mutex_unlock(&registry->lock);
            return &registry->nodes[i];
        }
    }

    pthread_mutex_unlock(&registry->lock);
    return NULL;
}

int set_election_priority(NodeRegistry *registry,
                         const char *node_id,
                         int priority) {
    (void)registry; (void)node_id; (void)priority;
    /* 简化实现 */
    return 0;
}

int force_reelection(NodeRegistry *registry, const ElectionConfig *config) {
    (void)registry; (void)config;
    /* 简化实现 */
    return 0;
}

/* ========================================================================
 * 配置管理
 * ======================================================================== */

ConfigManager *config_manager_create(const char *cluster_id, RaftServer *raft) {
    ConfigManager *mgr = (ConfigManager *)calloc(1, sizeof(ConfigManager));
    if (!mgr) return NULL;

    if (cluster_id) {
        snprintf(mgr->cluster_id, sizeof(mgr->cluster_id), "%s", cluster_id);
    }

    mgr->raft = raft;

    mgr->items_capacity = 64;
    mgr->items = (ConfigItem *)calloc(mgr->items_capacity, sizeof(ConfigItem));
    mgr->num_items = 0;

    pthread_mutex_init(&mgr->lock, NULL);

    /* 注册一些默认配置 */
    config_set(mgr, "cluster.name", cluster_id, "system");
    config_set(mgr, "cluster.timeout", "30000", "system");
    config_set(mgr, "cluster.replica_factor", "3", "system");

    return mgr;
}

void config_manager_destroy(ConfigManager *mgr) {
    if (!mgr) return;

    pthread_mutex_destroy(&mgr->lock);
    free(mgr->items);
    free(mgr);
}

const char *config_get(ConfigManager *mgr, const char *key, const char *default_value) {
    if (!mgr || !key) return default_value;

    pthread_mutex_lock(&mgr->lock);

    for (size_t i = 0; i < mgr->num_items; i++) {
        if (strcmp(mgr->items[i].key, key) == 0) {
            pthread_mutex_unlock(&mgr->lock);
            return mgr->items[i].value;
        }
    }

    pthread_mutex_unlock(&mgr->lock);
    return default_value;
}

int config_set(ConfigManager *mgr, const char *key, const char *value, const char *updater) {
    if (!mgr || !key || !value) return -1;

    pthread_mutex_lock(&mgr->lock);

    /* 查找或创建 */
    for (size_t i = 0; i < mgr->num_items; i++) {
        if (strcmp(mgr->items[i].key, key) == 0) {
            /* 更新现有 */
            mgr->items[i].version++;
            mgr->items[i].updated_at = get_current_time_ms();
            snprintf(mgr->items[i].value, sizeof(mgr->items[i].value), "%s", value);
            if (updater) {
                snprintf(mgr->items[i].updated_by, sizeof(mgr->items[i].updated_by), "%s", updater);
            }
            mgr->config_version++;
            pthread_mutex_unlock(&mgr->lock);
            return 0;
        }
    }

    /* 添加新配置 */
    if (mgr->num_items >= mgr->items_capacity) {
        mgr->items_capacity *= 2;
        mgr->items = (ConfigItem *)realloc(mgr->items, mgr->items_capacity * sizeof(ConfigItem));
    }

    ConfigItem *item = &mgr->items[mgr->num_items++];
    snprintf(item->key, sizeof(item->key), "%s", key);
    snprintf(item->value, sizeof(item->value), "%s", value);
    item->version = 1;
    item->updated_at = get_current_time_ms();
    if (updater) {
        snprintf(item->updated_by, sizeof(item->updated_by), "%s", updater);
    }

    mgr->config_version++;

    pthread_mutex_unlock(&mgr->lock);
    return 0;
}

int config_delete(ConfigManager *mgr, const char *key) {
    if (!mgr || !key) return -1;

    pthread_mutex_lock(&mgr->lock);

    for (size_t i = 0; i < mgr->num_items; i++) {
        if (strcmp(mgr->items[i].key, key) == 0) {
            for (size_t j = i; j < mgr->num_items - 1; j++) {
                mgr->items[j] = mgr->items[j + 1];
            }
            mgr->num_items--;
            mgr->config_version++;
            pthread_mutex_unlock(&mgr->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&mgr->lock);
    return -1;
}

const ConfigItem **config_list(ConfigManager *mgr, size_t *out_count) {
    if (!mgr || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    pthread_mutex_lock(&mgr->lock);
    *out_count = mgr->num_items;
    pthread_mutex_unlock(&mgr->lock);

    return (const ConfigItem **)mgr->items;
}

int config_batch_update(ConfigManager *mgr,
                        const char **keys,
                        const char **values,
                        size_t count,
                        const char *updater) {
    if (!mgr || !keys || !values) return -1;

    int success = 0;
    for (size_t i = 0; i < count; i++) {
        if (config_set(mgr, keys[i], values[i], updater) == 0) {
            success++;
        }
    }

    return success == (int)count ? 0 : success;
}

int config_subscribe(ConfigManager *mgr, ConfigChangeListener listener) {
    (void)mgr; (void)listener;
    /* 简化实现 */
    return 0;
}

uint64_t config_get_version(ConfigManager *mgr) {
    return mgr ? mgr->config_version : 0;
}

bool config_validate(const ConfigItem *item) {
    (void)item;
    /* 简化实现：总是返回 true */
    return true;
}

int config_rollback(ConfigManager *mgr, uint64_t target_version) {
    (void)mgr; (void)target_version;
    /* 简化实现 */
    return 0;
}

/* ========================================================================
 * 集群扩缩容
 * ======================================================================== */

RescaleTask *rescale_task_create(const char *task_id, bool is_expansion, size_t target_count) {
    RescaleTask *task = (RescaleTask *)calloc(1, sizeof(RescaleTask));
    if (!task) return NULL;

    if (task_id) {
        snprintf(task->task_id, sizeof(task->task_id), "%s", task_id);
    }

    task->is_expansion = is_expansion;
    task->target_node_count = target_count;
    task->state = RESCALE_PLANNING;
    task->start_time = get_current_time_ms();

    return task;
}

void rescale_task_update_progress(RescaleTask *task, size_t migrated, size_t total) {
    if (!task) return;

    task->shards_migrated = migrated;
    task->shards_total = total;
    task->progress = total > 0 ? (double)migrated / total * 100.0 : 0.0;

    if (task->state == RESCALE_PLANNING) {
        task->state = RESCALE_MIGRATING_DATA;
    }
}

void rescale_task_complete(RescaleTask *task) {
    if (!task) return;

    task->state = RESCALE_COMPLETE;
    task->progress = 100.0;
    task->actual_end = get_current_time_ms();
}

void rescale_task_fail(RescaleTask *task, const char *error) {
    if (!task) return;

    task->state = RESCALE_FAILED;
    task->actual_end = get_current_time_ms();
    if (error) {
        snprintf(task->error, sizeof(task->error), "%s", error);
    }
}

void rescale_task_destroy(RescaleTask *task) {
    if (!task) return;
    free(task->shards_to_move);
    free(task);
}

RescaleTask *cluster_expand(NodeRegistry *registry,
                           RaftRouter *router,
                           size_t target_nodes,
                           const RescalePolicy *policy) {
    (void)router; (void)policy;

    if (!registry) return NULL;

    /* 创建扩容任务 */
    RescaleTask *task = rescale_task_create("expand", true, target_nodes);
    if (!task) return NULL;

    task->source_node_count = registry->num_nodes;
    task->num_shards_to_move = 0;
    task->state = RESCALE_PREPARING;

    /* 模拟扩容 */
    task->shards_total = 1;
    task->shards_migrated = 1;
    rescale_task_complete(task);

    return task;
}

RescaleTask *cluster_shrink(NodeRegistry *registry,
                           RaftRouter *router,
                           size_t target_nodes,
                           const RescalePolicy *policy) {
    (void)router; (void)policy;

    if (!registry) return NULL;

    RescaleTask *task = rescale_task_create("shrink", false, target_nodes);
    if (!task) return NULL;

    task->source_node_count = registry->num_nodes;
    task->num_shards_to_move = 0;
    task->state = RESCALE_PREPARING;

    /* 模拟缩容 */
    task->shards_total = 1;
    task->shards_migrated = 1;
    rescale_task_complete(task);

    return task;
}

int cluster_add_node(NodeRegistry *registry,
                     RaftServer *raft,
                     const NodeMetadata *node_info) {
    (void)raft;

    if (!registry || !node_info) return -1;
    return node_registry_register(registry, node_info);
}

int cluster_remove_node(NodeRegistry *registry,
                        RaftServer *raft,
                        const char *node_id) {
    (void)raft;

    if (!registry || !node_id) return -1;
    return node_registry_unregister(registry, node_id);
}

int cluster_rebalance(NodeRegistry *registry,
                     RaftRouter *router,
                     const RescalePolicy *policy) {
    (void)registry; (void)router; (void)policy;
    /* 简化实现 */
    return 0;
}

bool cluster_needs_rescale(NodeRegistry *registry, const RescalePolicy *policy) {
    if (!registry || !policy) return false;

    if (policy->strategy == RESCALE_MANUAL) return false;

    /* 检查节点利用率 */
    pthread_mutex_lock(&registry->lock);

    double avg_cpu = 0;
    size_t active = 0;
    for (size_t i = 0; i < registry->num_nodes; i++) {
        if (registry->nodes[i].is_active) {
            avg_cpu += registry->nodes[i].metadata.cpu_usage;
            active++;
        }
    }

    pthread_mutex_unlock(&registry->lock);

    if (active > 0) {
        avg_cpu /= active;
        return avg_cpu > policy->max_cpu_usage;
    }

    return false;
}

void cluster_get_rescale_suggestion(NodeRegistry *registry,
                                    const RescalePolicy *policy,
                                    bool *out_need_scale,
                                    size_t *out_suggested_nodes) {
    if (out_need_scale) *out_need_scale = false;
    if (out_suggested_nodes) *out_suggested_nodes = 0;

    if (!registry || !policy) return;

    if (cluster_needs_rescale(registry, policy)) {
        if (out_need_scale) *out_need_scale = true;
        if (out_suggested_nodes) *out_suggested_nodes = registry->num_nodes + policy->scale_step;
    }
}

int rescale_task_cancel(RescaleTask *task) {
    if (!task) return -1;

    if (task->state == RESCALE_PLANNING || task->state == RESCALE_PREPARING) {
        task->state = RESCALE_FAILED;
        snprintf(task->error, sizeof(task->error), "Cancelled by user");
        return 0;
    }

    return -1;
}

/* ========================================================================
 * 服务发现
 * ======================================================================== */

ServiceDiscovery *service_discovery_create(void) {
    ServiceDiscovery *sd = (ServiceDiscovery *)calloc(1, sizeof(ServiceDiscovery));
    if (sd) {
        sd->num_endpoints = 0;
        pthread_mutex_init(&sd->lock, NULL);
    }
    return sd;
}

void service_discovery_destroy(ServiceDiscovery *sd) {
    if (!sd) return;

    pthread_mutex_destroy(&sd->lock);
    free(sd->endpoints);
    free(sd);
}

int service_discovery_register(ServiceDiscovery *sd, const ServiceEndpoint *endpoint) {
    if (!sd || !endpoint) return -1;

    pthread_mutex_lock(&sd->lock);

    if (!sd->endpoints) {
        sd->endpoints = (ServiceEndpoint *)calloc(16, sizeof(ServiceEndpoint));
    }

    sd->endpoints[sd->num_endpoints++] = *endpoint;

    pthread_mutex_unlock(&sd->lock);
    return 0;
}

int service_discovery_unregister(ServiceDiscovery *sd, const char *service_name, const char *node_id) {
    if (!sd || !service_name || !node_id) return -1;

    pthread_mutex_lock(&sd->lock);

    for (size_t i = 0; i < sd->num_endpoints; i++) {
        if (strcmp(sd->endpoints[i].service_name, service_name) == 0 &&
            strcmp(sd->endpoints[i].node_id, node_id) == 0) {
            for (size_t j = i; j < sd->num_endpoints - 1; j++) {
                sd->endpoints[j] = sd->endpoints[j + 1];
            }
            sd->num_endpoints--;
            pthread_mutex_unlock(&sd->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&sd->lock);
    return -1;
}

const ServiceEndpoint *service_discovery_find(ServiceDiscovery *sd,
                                             const char *service_name,
                                             size_t *out_count) {
    if (!sd || !service_name || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    pthread_mutex_lock(&sd->lock);

    static ServiceEndpoint results[256];
    size_t count = 0;

    for (size_t i = 0; i < sd->num_endpoints && count < 256; i++) {
        if (strcmp(sd->endpoints[i].service_name, service_name) == 0 &&
            sd->endpoints[i].is_healthy) {
            results[count++] = sd->endpoints[i];
        }
    }

    pthread_mutex_unlock(&sd->lock);

    *out_count = count;
    return results;
}

const ServiceEndpoint *service_discovery_find_nearest(ServiceDiscovery *sd,
                                                     const char *service_name,
                                                     const char *from_node_id) {
    (void)from_node_id;

    size_t count = 0;
    const ServiceEndpoint *endpoints = service_discovery_find(sd, service_name, &count);

    if (count > 0) {
        /* 简化：返回第一个 */
        return &endpoints[0];
    }

    return NULL;
}

int service_discovery_health_check(ServiceDiscovery *sd) {
    if (!sd) return -1;

    pthread_mutex_lock(&sd->lock);

    /* 简化：标记所有端点为健康 */
    for (size_t i = 0; i < sd->num_endpoints; i++) {
        sd->endpoints[i].is_healthy = true;
    }

    pthread_mutex_unlock(&sd->lock);
    return 0;
}
