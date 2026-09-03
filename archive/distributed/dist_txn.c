/**
 * @file dist_txn.c
 * @brief 分布式事务核心实现
 */

#include "db/core/dist_txn.h"
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
 * 分布式事务
 * ======================================================================== */

DistTxn *tpc_begin(TwoPCoordinator *coord, uint64_t txn_id) {
    if (!coord) return NULL;

    DistTxn *txn = (DistTxn *)calloc(1, sizeof(DistTxn));
    if (!txn) return NULL;

    txn->txn_id.txn_id = txn_id;
    snprintf(txn->txn_id.coordinator_id, sizeof(txn->txn_id.coordinator_id), "%s", coord->coordinator_id);
    txn->txn_id.start_time = get_current_time_ms();

    txn->state = DIST_TXN_ACTIVE;
    txn->is_coordinator = true;
    txn->start_time = get_current_time_ms();

    /* 添加到活跃事务 */
    pthread_mutex_lock(&coord->lock);

    if (coord->num_active_txns >= coord->active_capacity) {
        coord->active_capacity *= 2;
        coord->active_txns = (DistTxn **)realloc(coord->active_txns,
                                                  coord->active_capacity * sizeof(DistTxn *));
    }
    coord->active_txns[coord->num_active_txns++] = txn;

    pthread_mutex_unlock(&coord->lock);

    return txn;
}

int tpc_add_participant(DistTxn *txn, const char *node_id, const char *address, int port) {
    if (!txn || !node_id) return -1;

    if (!txn->participants) {
        txn->participants_capacity = 4;
        txn->participants = (TxnParticipant *)calloc(txn->participants_capacity,
                                                     sizeof(TxnParticipant));
    }

    if (txn->num_participants >= txn->participants_capacity) {
        txn->participants_capacity *= 2;
        txn->participants = (TxnParticipant *)realloc(txn->participants,
                                                        txn->participants_capacity * sizeof(TxnParticipant));
    }

    TxnParticipant *p = &txn->participants[txn->num_participants++];
    snprintf(p->node_id, sizeof(p->node_id), "%s", node_id);
    if (address) snprintf(p->address, sizeof(p->address), "%s", address);
    p->port = port;
    p->state = PARTICIPANT_INIT;

    return 0;
}

DistTxnState tpc_get_txn_state(TwoPCoordinator *coord, const GlobalTxnId *txn_id) {
    if (!coord || !txn_id) return DIST_TXN_UNKNOWN;

    pthread_mutex_lock(&coord->lock);

    for (size_t i = 0; i < coord->num_active_txns; i++) {
        DistTxn *txn = coord->active_txns[i];
        if (txn->txn_id.txn_id == txn_id->txn_id &&
            strcmp(txn->txn_id.coordinator_id, txn_id->coordinator_id) == 0) {
            pthread_mutex_unlock(&coord->lock);
            return txn->state;
        }
    }

    pthread_mutex_unlock(&coord->lock);
    return DIST_TXN_UNKNOWN;
}

/* ========================================================================
 * 2PC 协调者
 * ======================================================================== */

TwoPCoordinator *tpc_coordinator_create(const char *coordinator_id, ShardRouter *router) {
    TwoPCoordinator *coord = (TwoPCoordinator *)calloc(1, sizeof(TwoPCoordinator));
    if (!coord) return NULL;

    if (coordinator_id) {
        snprintf(coord->coordinator_id, sizeof(coord->coordinator_id), "%s", coordinator_id);
    }

    coord->router = router;

    /* 初始化活跃事务数组 */
    coord->active_capacity = 16;
    coord->active_txns = (DistTxn **)calloc(coord->active_capacity, sizeof(DistTxn *));

    /* 初始化互斥锁 */
    pthread_mutex_init(&coord->lock, NULL);

    /* 默认超时配置 */
    coord->prepare_timeout_ms = 30000;   /* 30 秒 */
    coord->commit_timeout_ms = 10000;    /* 10 秒 */
    coord->heartbeat_interval_ms = 5000; /* 5 秒 */

    return coord;
}

void tpc_coordinator_destroy(TwoPCoordinator *coord) {
    if (!coord) return;

    pthread_mutex_lock(&coord->lock);

    /* 清理活跃事务 */
    for (size_t i = 0; i < coord->num_active_txns; i++) {
        DistTxn *txn = coord->active_txns[i];
        if (txn) {
            free(txn->participants);
            free(txn->held_locks);
            free(txn);
        }
    }
    free(coord->active_txns);

    pthread_mutex_unlock(&coord->lock);
    pthread_mutex_destroy(&coord->lock);

    free(coord);
}

int tpc_prepare(TwoPCoordinator *coord, DistTxn *txn) {
    if (!coord || !txn) return -1;

    txn->state = DIST_TXN_PREPARING;

    /* 向所有参与者发送 Prepare 请求 */
    for (size_t i = 0; i < txn->num_participants; i++) {
        TxnParticipant *p = &txn->participants[i];

        /* 简化：假设 Prepare 成功 */
        p->state = PARTICIPANT_PREPARED;
    }

    txn->state = DIST_TXN_PREPARED;
    txn->prepare_time = get_current_time_ms();

    return 0;
}

int tpc_commit(TwoPCoordinator *coord, DistTxn *txn) {
    if (!coord || !txn) return -1;

    txn->state = DIST_TXN_COMMITTING;

    /* 向所有参与者发送 Commit 请求 */
    for (size_t i = 0; i < txn->num_participants; i++) {
        TxnParticipant *p = &txn->participants[i];

        if (p->state == PARTICIPANT_PREPARED) {
            /* 简化：假设 Commit 成功 */
            p->state = PARTICIPANT_COMMITTED;
        }
    }

    txn->state = DIST_TXN_COMMITTED;
    txn->txn_id.commit_time = get_current_time_ms();
    txn->commit_time = txn->txn_id.commit_time;

    /* 从活跃事务中移除 */
    pthread_mutex_lock(&coord->lock);

    for (size_t i = 0; i < coord->num_active_txns; i++) {
        if (coord->active_txns[i] == txn) {
            /* 移动后面的元素 */
            for (size_t j = i; j < coord->num_active_txns - 1; j++) {
                coord->active_txns[j] = coord->active_txns[j + 1];
            }
            coord->num_active_txns--;
            break;
        }
    }

    pthread_mutex_unlock(&coord->lock);

    /* 清理事务 */
    free(txn->participants);
    free(txn->held_locks);
    free(txn);

    return 0;
}

int tpc_abort(TwoPCoordinator *coord, DistTxn *txn) {
    if (!coord || !txn) return -1;

    txn->state = DIST_TXN_ABORTING;

    /* 向所有参与者发送 Abort 请求 */
    for (size_t i = 0; i < txn->num_participants; i++) {
        TxnParticipant *p = &txn->participants[i];

        if (p->state != PARTICIPANT_COMMITTED) {
            /* 简化：假设 Abort 成功 */
            p->state = PARTICIPANT_ABORTED;
        }
    }

    txn->state = DIST_TXN_ABORTED;

    /* 从活跃事务中移除 */
    pthread_mutex_lock(&coord->lock);

    for (size_t i = 0; i < coord->num_active_txns; i++) {
        if (coord->active_txns[i] == txn) {
            for (size_t j = i; j < coord->num_active_txns - 1; j++) {
                coord->active_txns[j] = coord->active_txns[j + 1];
            }
            coord->num_active_txns--;
            break;
        }
    }

    pthread_mutex_unlock(&coord->lock);

    /* 清理事务 */
    free(txn->participants);
    free(txn->held_locks);
    free(txn);

    return 0;
}

int tpc_single_phase_commit(TwoPCoordinator *coord, DistTxn *txn) {
    if (!coord || !txn) return -1;

    /* 简化：直接提交 */
    txn->state = DIST_TXN_COMMITTED;
    txn->commit_time = get_current_time_ms();

    return 0;
}

void tpc_check_timeouts(TwoPCoordinator *coord) {
    if (!coord) return;

    uint64_t now = get_current_time_ms();

    pthread_mutex_lock(&coord->lock);

    for (size_t i = 0; i < coord->num_active_txns; i++) {
        DistTxn *txn = coord->active_txns[i];

        /* 检查超时 */
        uint64_t elapsed = now - txn->start_time;
        if (elapsed > coord->prepare_timeout_ms && txn->state == DIST_TXN_PREPARING) {
            /* Prepare 超时，回滚 */
            pthread_mutex_unlock(&coord->lock);
            tpc_abort(coord, txn);
            pthread_mutex_lock(&coord->lock);
            i--;  /* 重新检查 */
        }
    }

    pthread_mutex_unlock(&coord->lock);
}

int tpc_recover_pending(TwoPCoordinator *coord) {
    if (!coord) return -1;

    /* 简化：扫描日志恢复未决事务 */
    pthread_mutex_lock(&coord->lock);

    /* 遍历活跃事务，检查是否有 PREPARED 但超时的 */
    for (size_t i = 0; i < coord->num_active_txns; i++) {
        DistTxn *txn = coord->active_txns[i];
        if (txn->state == DIST_TXN_PREPARED) {
            /* 尝试提交 */
            pthread_mutex_unlock(&coord->lock);
            tpc_commit(coord, txn);
            pthread_mutex_lock(&coord->lock);
            i--;
        }
    }

    pthread_mutex_unlock(&coord->lock);

    return 0;
}

/* ========================================================================
 * 2PC 参与者
 * ======================================================================== */

TwoPCParticipant *tpc_participant_create(const char *node_id) {
    TwoPCParticipant *part = (TwoPCParticipant *)calloc(1, sizeof(TwoPCParticipant));
    if (!part) return NULL;

    if (node_id) {
        snprintf(part->node_id, sizeof(part->node_id), "%s", node_id);
    }

    pthread_mutex_init(&part->lock, NULL);

    return part;
}

void tpc_participant_destroy(TwoPCParticipant *part) {
    if (!part) return;

    pthread_mutex_destroy(&part->lock);
    free(part);
}

int tpc_vote_prepare(TwoPCParticipant *part, const GlobalTxnId *txn_id, bool vote_yes) {
    (void)part; (void)txn_id;

    /* 简化：总是投赞成票 */
    return vote_yes ? 0 : -1;
}

int tpc_local_commit(TwoPCParticipant *part, const GlobalTxnId *txn_id) {
    (void)part; (void)txn_id;
    /* 简化实现 */
    return 0;
}

int tpc_local_abort(TwoPCParticipant *part, const GlobalTxnId *txn_id) {
    (void)part; (void)txn_id;
    /* 简化实现 */
    return 0;
}

int tpc_reconnect_coordinator(TwoPCParticipant *part) {
    (void)part;
    /* 简化实现 */
    return 0;
}

/* ========================================================================
 * SAGA 模式
 * ======================================================================== */

SagaOrchestrator *saga_create(const char *saga_id, size_t max_steps) {
    SagaOrchestrator *saga = (SagaOrchestrator *)calloc(1, sizeof(SagaOrchestrator));
    if (!saga) return NULL;

    if (saga_id) {
        snprintf(saga->saga_id, sizeof(saga->saga_id), "%s", saga_id);
    }

    saga->steps = (SagaStep *)calloc(max_steps, sizeof(SagaStep));
    saga->num_steps = 0;
    saga->current_step = 0;
    saga->direction = SAGA_FORWARD;
    saga->in_progress = false;

    return saga;
}

int saga_add_step(SagaOrchestrator *saga,
                  const char *step_name,
                  const char *service_name,
                  int (*forward_fn)(void *),
                  int (*compensate_fn)(void *)) {
    if (!saga || !step_name) return -1;

    SagaStep *step = &saga->steps[saga->num_steps++];
    snprintf(step->step_name, sizeof(step->step_name), "%s", step_name);
    if (service_name) snprintf(step->service_name, sizeof(step->service_name), "%s", service_name);
    step->forward_fn = forward_fn;
    step->compensate_fn = compensate_fn;
    step->completed = false;
    step->compensated = false;

    return 0;
}

int saga_execute(SagaOrchestrator *saga, void *initial_ctx) {
    if (!saga) return -1;

    saga->in_progress = true;
    saga->direction = SAGA_FORWARD;

    void *ctx = initial_ctx;

    /* 正向执行每个步骤 */
    for (size_t i = 0; i < saga->num_steps; i++) {
        SagaStep *step = &saga->steps[i];

        if (step->forward_fn) {
            int ret = step->forward_fn(ctx);
            if (ret != 0) {
                /* 执行失败，启动补偿 */
                saga_rollback(saga);
                return ret;
            }
        }
        step->completed = true;
        saga->completed_steps++;
    }

    saga->in_progress = false;
    return 0;
}

int saga_rollback(SagaOrchestrator *saga) {
    if (!saga) return -1;

    saga->direction = SAGA_BACKWARD;
    saga->in_progress = true;

    /* 反向补偿已完成的步骤 */
    for (int i = (int)saga->completed_steps - 1; i >= 0; i--) {
        SagaStep *step = &saga->steps[i];

        if (step->completed && !step->compensated && step->compensate_fn) {
            int ret = step->compensate_fn(step->context);
            if (ret == 0) {
                step->compensated = true;
                saga->compensated_steps++;
            }
        }
    }

    saga->in_progress = false;
    return 0;
}

const char *saga_get_status(const SagaOrchestrator *saga) {
    if (!saga) return "NULL";

    if (saga->in_progress) {
        if (saga->direction == SAGA_FORWARD) {
            return "IN_PROGRESS";
        } else {
            return "ROLLING_BACK";
        }
    }

    if (saga->compensated_steps == saga->completed_steps) {
        return "COMPENSATED";
    }

    return "COMPLETED";
}

void saga_destroy(SagaOrchestrator *saga) {
    if (!saga) return;
    free(saga->steps);
    free(saga);
}

/* ========================================================================
 * TSO (Timestamp Oracle)
 * ======================================================================== */

TSOClient *tso_client_create(const char *server_addr, int server_port) {
    TSOClient *client = (TSOClient *)calloc(1, sizeof(TSOClient));
    if (!client) return NULL;

    if (server_addr) {
        snprintf(client->server_addr, sizeof(client->server_addr), "%s", server_addr);
    }
    client->server_port = server_port;
    client->last_timestamp = 0;
    client->last_epoch = 0;
    client->connection_fd = -1;

    return client;
}

uint64_t tso_get_timestamp(TSOClient *client) {
    if (!client) return 0;

    /* 简化：生成逻辑时间戳 */
    static uint64_t logical = 0;
    logical++;

    /* 组合物理时间和逻辑时间 */
    uint64_t physical = (uint64_t)(time(NULL)) * 1000;
    uint64_t timestamp = (physical << 18) | (logical & ((1 << 18) - 1));

    client->last_timestamp = timestamp;
    return timestamp;
}

int tso_get_batch(TSOClient *client, uint64_t *timestamps, size_t count) {
    if (!client || !timestamps) return -1;

    uint64_t base = tso_get_timestamp(client);
    for (size_t i = 0; i < count; i++) {
        timestamps[i] = base + i;
    }

    return 0;
}

void tso_client_destroy(TSOClient *client) {
    free(client);
}

uint64_t tso_generate_local(TSOConfig *config) {
    if (!config) return 0;

    uint64_t physical = (uint64_t)(time(NULL)) * 1000;

    /* 如果物理时间没有前进，使用逻辑计数器 */
    if (physical <= config->physical_time) {
        config->logical_counter++;
        if (config->logical_counter >= config->max_logical) {
            config->logical_counter = 0;
        }
        return (config->physical_time << 18) | (config->logical_counter & ((1 << 18) - 1));
    }

    /* 物理时间前进，重置逻辑计数器 */
    config->physical_time = physical;
    config->logical_counter = 0;

    return (physical << 18);
}

int tso_sync(TSOConfig *config, const char *server_addr, int server_port) {
    (void)config; (void)server_addr; (void)server_port;
    /* 简化实现 */
    return 0;
}

/* ========================================================================
 * 分布式 MVCC
 * ======================================================================== */

DistReadView *dist_readview_create(uint64_t snapshot_time,
                                   const GlobalTxnId *active_txns,
                                   size_t num_active,
                                   uint32_t origin_node) {
    DistReadView *view = (DistReadView *)calloc(1, sizeof(DistReadView));
    if (!view) return NULL;

    view->snapshot_time = snapshot_time;
    view->origin_node = origin_node;
    view->created_at = get_current_time_ms();

    if (active_txns && num_active > 0) {
        view->active_txns = (GlobalTxnId *)malloc(num_active * sizeof(GlobalTxnId));
        if (view->active_txns) {
            memcpy(view->active_txns, active_txns, num_active * sizeof(GlobalTxnId));
            view->num_active = num_active;
        }
    }

    /* 简化：计算 min/max active */
    if (num_active > 0) {
        view->min_active_txn = active_txns[0].txn_id;
        view->max_active_txn = active_txns[num_active - 1].txn_id;
    }

    return view;
}

bool dist_version_visible(const DistReadView *view, const DistVersionChain *version) {
    if (!view || !version) return false;

    /* xmax 已设置表示被删除 */
    if (version->xmax != 0 && version->xmax != UINT64_MAX) {
        /* 检查 xmax 是否在快照中可见 */
        if (version->xmax <= view->snapshot_time) {
            return false;  /* 被删除且删除事务已提交 */
        }
    }

    /* xmin 必须在快照之前或属于已提交事务 */
    if (version->xmin > view->snapshot_time) {
        return false;  /* 创建事务还未提交 */
    }

    /* 检查是否是活跃事务创建的 */
    for (size_t i = 0; i < view->num_active; i++) {
        if (view->active_txns[i].txn_id == version->xmin) {
            return false;  /* 由当前快照的活跃事务创建 */
        }
    }

    return true;
}

const DistVersionChain *dist_get_visible_version(DistReadView *view,
                                                   uint64_t row_id,
                                                   uint32_t shard_id) {
    (void)row_id; (void)shard_id;

    static DistVersionChain dummy_version;

    /* 简化实现：返回虚拟版本 */
    dummy_version.xmin = 1;
    dummy_version.xmax = 0;
    dummy_version.origin_node = view ? view->origin_node : 0;

    return &dummy_version;
}

void dist_readview_destroy(DistReadView *view) {
    if (!view) return;
    free(view->active_txns);
    free(view);
}

/* ========================================================================
 * 分布式快照
 * ======================================================================== */

DistSnapshot *dist_snapshot_create(TSOClient *tso,
                                   const GlobalTxnId *active_txns,
                                   size_t num_active,
                                   uint32_t origin_node) {
    DistSnapshot *snap = (DistSnapshot *)calloc(1, sizeof(DistSnapshot));
    if (!snap) return NULL;

    snap->snapshot_id = tso ? tso_get_timestamp(tso) : 0;
    snap->snapshot_time = snap->snapshot_id;
    snap->local_time = get_current_time_ms();
    snap->origin_node = origin_node;
    snap->valid_until = snap->local_time + 300000;  /* 5 分钟有效期 */

    if (active_txns && num_active > 0) {
        snap->global_active = (GlobalTxnId *)malloc(num_active * sizeof(GlobalTxnId));
        if (snap->global_active) {
            memcpy(snap->global_active, active_txns, num_active * sizeof(GlobalTxnId));
            snap->num_global_active = num_active;
        }
    }

    return snap;
}

bool dist_snapshot_valid(const DistSnapshot *snapshot) {
    if (!snapshot) return false;
    return get_current_time_ms() < snapshot->valid_until;
}

uint64_t dist_snapshot_get_time(const DistSnapshot *snapshot) {
    return snapshot ? snapshot->snapshot_time : 0;
}

void dist_snapshot_destroy(DistSnapshot *snapshot) {
    if (!snapshot) return;
    free(snapshot->global_active);
    free(snapshot);
}

/* ========================================================================
 * 故障恢复
 * ======================================================================== */

RecoveryReport *dist_recover_coordinator(TwoPCoordinator *coord) {
    RecoveryReport *report = (RecoveryReport *)calloc(1, sizeof(RecoveryReport));
    if (!report) return NULL;

    report->state = RECOVERY_INIT;
    report->start_time = get_current_time_ms();

    /* 扫描日志 */
    report->state = RECOVERY_SCAN_LOG;

    /* 重建状态 */
    report->state = RECOVERY_REBUILD_STATE;

    /* 重放 Prepared 事务 */
    report->state = RECOVERY_REPLAY_PREPARED;

    /* 统计 */
    report->total_prepared = 0;
    report->committed_count = 0;
    report->aborted_count = 0;
    report->unknown_count = 0;

    report->state = RECOVERY_COMPLETE;
    report->end_time = get_current_time_ms();

    return report;
}

RecoveryReport *dist_recover_participant(TwoPCParticipant *part) {
    RecoveryReport *report = (RecoveryReport *)calloc(1, sizeof(RecoveryReport));
    if (!report) return NULL;

    report->state = RECOVERY_INIT;
    report->start_time = get_current_time_ms();

    /* 简化：模拟恢复过程 */
    report->state = RECOVERY_COMPLETE;
    report->end_time = get_current_time_ms();

    (void)part;
    return report;
}

int dist_resolve_unknown_txn(TwoPCoordinator *coord, const GlobalTxnId *txn_id) {
    (void)coord; (void)txn_id;
    /* 简化：假设超时则回滚 */
    return 0;
}

GlobalTxnId *dist_scan_prepared_log(void *log, size_t *out_count) {
    (void)log;
    if (out_count) *out_count = 0;
    return NULL;  /* 简化：暂无日志 */
}

int dist_cleanup_old_data(DistTxn *txn, uint64_t before_time) {
    (void)txn; (void)before_time;
    return 0;
}

/* ========================================================================
 * 事务管理器
 * ======================================================================== */

DistTxnManager *dist_txn_manager_create(const char *node_id, bool is_coordinator) {
    DistTxnManager *mgr = (DistTxnManager *)calloc(1, sizeof(DistTxnManager));
    if (!mgr) return NULL;

    mgr->is_single_node = !is_coordinator;

    /* 创建协调者或参与者 */
    if (is_coordinator) {
        mgr->coordinator = tpc_coordinator_create(node_id, NULL);
    } else {
        mgr->participant = tpc_participant_create(node_id);
    }

    /* 默认配置 */
    mgr->txn_timeout_ms = 60000;    /* 60 秒 */
    mgr->lock_timeout_ms = 30000;    /* 30 秒 */

    return mgr;
}

int dist_txn_manager_init(DistTxnManager *mgr,
                         const char *tso_addr,
                         int tso_port,
                         ShardRouter *router) {
    if (!mgr) return -1;

    /* 初始化 TSO 客户端 */
    if (tso_addr) {
        mgr->tso_client = tso_client_create(tso_addr, tso_port);
    }

    /* 设置路由 */
    if (mgr->coordinator && router) {
        mgr->coordinator->router = router;
    }

    return 0;
}

DistTxn *dist_txn_begin(DistTxnManager *mgr) {
    if (!mgr) return NULL;

    if (mgr->coordinator) {
        static uint64_t txn_counter = 0;
        return tpc_begin(mgr->coordinator, ++txn_counter);
    }

    return NULL;
}

int dist_txn_commit(DistTxn *txn) {
    if (!txn) return -1;

    /* 通过全局事务 ID 找到协调者提交 */
    /* 简化实现 */
    txn->state = DIST_TXN_COMMITTED;
    return 0;
}

int dist_txn_rollback(DistTxn *txn) {
    if (!txn) return -1;

    txn->state = DIST_TXN_ABORTED;
    return 0;
}

void dist_txn_manager_destroy(DistTxnManager *mgr) {
    if (!mgr) return;

    if (mgr->coordinator) {
        tpc_coordinator_destroy(mgr->coordinator);
    }

    if (mgr->participant) {
        tpc_participant_destroy(mgr->participant);
    }

    if (mgr->tso_client) {
        tso_client_destroy(mgr->tso_client);
    }

    free(mgr);
}
