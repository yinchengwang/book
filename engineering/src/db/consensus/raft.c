/**
 * @file raft.c
 * @brief Raft 实现（工程层 Phase11）
 *
 * **核心策略**：
 * - 单 RaftServer 持有完整状态（term、role、log、timers）
 * - pthread_mutex 保护所有读写
 * - tick() 处理超时：follower → candidate → leader
 * - leader 提交日志按 round-trip 推进：submit() append log，tick() commit 单步
 * - **不实现集群通信**：单节点 smoke 测试可工作
 *
 * **tick() 伪代码**：
 * 1. 推进虚拟时间（advance_ms 或 wall-clock）
 * 2. follower 若 last_heartbeat 超时 → 转 candidate
 * 3. candidate 若 election_timeout 触发 → 自增 term、投自己一票、记 election_deadline
 * 4. leader：仅是占位（实际应发送心跳；smoke 阶段不需要）
 * 5. 若有 uncommitted log → commit_index++
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>

#include "db/consensus/raft.h"

#ifndef DEFAULT_ELECTION_TIMEOUT_MS
#define DEFAULT_ELECTION_TIMEOUT_MS 1000
#endif

struct RaftServer {
    RaftServerConfig_t config;
    pthread_mutex_t lock;

    /* 持久化状态 */
    uint64_t current_term;
    uint64_t voted_for;

    /* volatile */
    RaftRole_t role;
    uint64_t commit_index;
    uint64_t last_applied;
    uint64_t leader_id;

    /* 日志（仅 leader append） */
    RaftLogEntry_t *log;
    size_t log_size;
    size_t log_capacity;

    /* 时序 */
    uint64_t last_tick_ms;
    uint64_t last_election_reset_ms;

    /* P8: 虚拟时钟（advance_ms 推进，tick 用此而非 real clock） */
    uint64_t virtual_now_ms;
    bool use_virtual_clock;

    /* P5 持久化 */
    char *state_path;          /* NULL = in-memory */
    pthread_mutex_t save_lock;
    bool dirty;

    /* P7 Snapshot */
    void *snapshot_data;
    size_t snapshot_size;
    bool has_snapshot;
    uint64_t snapshot_last_index;
    uint64_t snapshot_last_term;

    /* P7 日志压缩偏移：array[i] 对应逻辑索引 i + first_log_index */
    uint64_t first_log_index;
};

/* ============================================================
 * 内部 helpers
 * ============================================================ */

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static uint32_t random_election_timeout(const RaftServer_t *srv) {
    uint32_t lo = srv->config.election_timeout_min_ms
                  ? srv->config.election_timeout_min_ms : DEFAULT_ELECTION_TIMEOUT_MS;
    uint32_t hi = srv->config.election_timeout_max_ms;
    if (hi <= lo) hi = lo + 500;
    /* 简化：返回中点（避免随机数依赖） */
    return lo + (hi - lo) / 2;
}

static int log_append(RaftServer_t *srv, uint64_t term, const void *data, size_t size) {
    if (srv->log_size == srv->log_capacity) {
        size_t new_cap = srv->log_capacity * 2 ? srv->log_capacity * 2 : 16;
        RaftLogEntry_t *new_log = (RaftLogEntry_t *)realloc(srv->log, new_cap * sizeof(RaftLogEntry_t));
        if (!new_log) return -1;
        srv->log = new_log;
        srv->log_capacity = new_cap;
    }
    RaftLogEntry_t *e = &srv->log[srv->log_size];
    e->term = term;
    e->index = srv->first_log_index + (uint64_t)srv->log_size;  /* 全局连续索引 */
    if (size > 0) {
        e->data = malloc(size);
        if (!e->data) return -1;
        memcpy(e->data, data, size);
        e->data_size = size;
    } else {
        e->data = NULL;
        e->data_size = 0;
    }
    srv->log_size++;
    return 0;
}

static void log_free_all(RaftServer_t *srv) {
    if (!srv->log) return;
    for (size_t i = 0; i < srv->log_size; i++) {
        free(srv->log[i].data);
    }
    free(srv->log);
    srv->log = NULL;
    srv->log_size = srv->log_capacity = 0;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

RaftServer_t *raft_server_create(const RaftServerConfig_t *cfg) {
    if (!cfg || cfg->node_id == UINT64_MAX) return NULL;

    RaftServer_t *srv = (RaftServer_t *)calloc(1, sizeof(RaftServer_t));
    if (!srv) return NULL;

    srv->config = *cfg;
    pthread_mutex_init(&srv->lock, NULL);

    srv->role = RAFT_ROLE_FOLLOWER;
    srv->current_term = 0;
    srv->voted_for = RAFT_NO_VOTE;
    srv->commit_index = 0;
    srv->last_applied = 0;
    srv->leader_id = RAFT_UNKNOWN_LEADER;

    srv->last_tick_ms = 0;
    srv->last_election_reset_ms = 0;

    srv->virtual_now_ms = 0;
    srv->use_virtual_clock = false;

    pthread_mutex_init(&srv->save_lock, NULL);
    srv->state_path = NULL;
    srv->dirty = false;

    srv->snapshot_data = NULL;
    srv->snapshot_size = 0;
    srv->has_snapshot = false;
    srv->snapshot_last_index = 0;
    srv->snapshot_last_term = 0;
    srv->first_log_index = 1;  /* Raft 约定 1-based */

    return srv;
}

RaftServer_t *raft_server_create_ex(const RaftServerConfig_t *cfg,
                                    const RaftStateConfig_t *state) {
    RaftServer_t *srv = raft_server_create(cfg);
    if (!srv) return NULL;

    if (state && state->state_path) {
        srv->state_path = strdup(state->state_path);
        /* 尝试 load（如果文件存在） */
        if (raft_server_load_state(srv, srv->state_path) != 0) {
            /* 不存在或读失败——保持初始状态 */
        }
    }
    return srv;
}

int raft_server_start(RaftServer_t *srv) {
    if (!srv) return -1;
    pthread_mutex_lock(&srv->lock);
    srv->last_election_reset_ms = srv->use_virtual_clock ? srv->virtual_now_ms : now_ms();
    pthread_mutex_unlock(&srv->lock);
    return 0;
}

int raft_server_stop(RaftServer_t *srv) {
    if (!srv) return -1;
    pthread_mutex_lock(&srv->lock);
    /* Phase11: noop（停止状态机线程，P1+ 续做） */
    pthread_mutex_unlock(&srv->lock);
    return 0;
}

void raft_server_destroy(RaftServer_t *srv) {
    if (!srv) return;
    pthread_mutex_lock(&srv->lock);
    log_free_all(srv);
    free(srv->snapshot_data);
    free(srv->state_path);
    pthread_mutex_unlock(&srv->lock);
    pthread_mutex_destroy(&srv->lock);
    pthread_mutex_destroy(&srv->save_lock);
    free(srv);
}

/* ============================================================
 * tick()
 * ============================================================ */

void raft_tick(RaftServer_t *srv) {
    if (!srv) return;

    pthread_mutex_lock(&srv->lock);

    uint64_t now = srv->use_virtual_clock ? srv->virtual_now_ms : now_ms();
    uint64_t elapsed = now - srv->last_election_reset_ms;
    uint32_t timeout = random_election_timeout(srv);

    /* follower 选举超时 → 转 candidate */
    if (srv->role == RAFT_ROLE_FOLLOWER && elapsed >= timeout) {
        srv->role = RAFT_ROLE_CANDIDATE;
    }

    /* candidate 阶段：自增 term、投自己一票、重置 deadline */
    if (srv->role == RAFT_ROLE_CANDIDATE) {
        srv->current_term++;
        srv->voted_for = srv->config.node_id;
        srv->leader_id = RAFT_UNKNOWN_LEADER;
        srv->last_election_reset_ms = now;

        if (srv->config.cluster_size == 1) {
            srv->role = RAFT_ROLE_LEADER;
            srv->leader_id = srv->config.node_id;
        }
    }

    /* leader：尝试推进 commit */
    if (srv->role == RAFT_ROLE_LEADER) {
        uint64_t max_global_idx = srv->first_log_index + (uint64_t)srv->log_size - 1;
        if (max_global_idx > srv->commit_index) {
            srv->commit_index = max_global_idx;
            srv->last_applied = srv->commit_index;
        }
    }

    srv->last_tick_ms = now;
    pthread_mutex_unlock(&srv->lock);
}

void raft_tick_advance_ms(RaftServer_t *srv, uint64_t ms) {
    if (!srv) return;
    pthread_mutex_lock(&srv->lock);
    /* 启用虚拟时钟 + 推进 */
    srv->use_virtual_clock = true;
    srv->virtual_now_ms += ms;
    pthread_mutex_unlock(&srv->lock);
}

/* ============================================================
 * 查询
 * ============================================================ */

RaftRole_t raft_get_role(const RaftServer_t *srv) {
    if (!srv) return RAFT_ROLE_FOLLOWER;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);  /* cast 兼容 const */
    RaftRole_t r = srv->role;
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return r;
}

uint64_t raft_get_current_term(const RaftServer_t *srv) {
    if (!srv) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    uint64_t t = srv->current_term;
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return t;
}

uint64_t raft_get_leader_id(const RaftServer_t *srv) {
    if (!srv) return RAFT_UNKNOWN_LEADER;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    uint64_t id = srv->leader_id;
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return id;
}

uint64_t raft_get_commit_index(const RaftServer_t *srv) {
    if (!srv) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    uint64_t ci = srv->commit_index;
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return ci;
}

uint64_t raft_get_last_applied(const RaftServer_t *srv) {
    if (!srv) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    uint64_t la = srv->last_applied;
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return la;
}

bool raft_is_leader(const RaftServer_t *srv) {
    if (!srv) return false;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    bool l = (srv->role == RAFT_ROLE_LEADER);
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return l;
}

/* ============================================================
 * 日志
 * ============================================================ */

size_t raft_submit(RaftServer_t *srv, const void *data, size_t size) {
    if (!srv) return SIZE_MAX;

    pthread_mutex_lock(&srv->lock);
    if (srv->role != RAFT_ROLE_LEADER) {
        pthread_mutex_unlock(&srv->lock);
        return SIZE_MAX;
    }
    int rc = log_append(srv, srv->current_term, data, size);
    size_t new_index;
    if (rc == 0) {
        /* 返回全局连续索引 */
        new_index = srv->first_log_index + (uint64_t)srv->log_size - 1;
        /* 单节点立即 commit */
        srv->commit_index = (uint64_t)new_index;
        srv->last_applied = srv->commit_index;
    } else {
        new_index = SIZE_MAX;
    }
    pthread_mutex_unlock(&srv->lock);
    return new_index;
}

size_t raft_get_committed(const RaftServer_t *srv, size_t idx,
                          void *buf, size_t cap) {
    if (!srv || !buf) return 0;

    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    size_t copy = 0;
    /* idx 是全局逻辑索引，需转换到物理数组偏移 */
    /* idx=0 特殊处理：从第一条日志开始（兼容旧调用） */
    uint64_t start_idx = (idx == 0) ? srv->first_log_index : (uint64_t)idx;
    if (start_idx >= srv->first_log_index) {
        size_t phys_idx = (size_t)(start_idx - srv->first_log_index);
        if (phys_idx < srv->log_size) {
            size_t available = srv->log_size - phys_idx;
            for (size_t i = 0; i < available && copy < cap; i++) {
                RaftLogEntry_t *e = &srv->log[phys_idx + i];
                size_t n = e->data_size;
                if (copy + n > cap) n = cap - copy;
                memcpy((char *)buf + copy, e->data, n);
                copy += n;
            }
        }
    }
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return copy;
}

/* ============================================================
 * 测试钩子
 * ============================================================ */

void raft_test_force_election_timeout(RaftServer_t *srv) {
    if (!srv) return;
    pthread_mutex_lock(&srv->lock);
    srv->last_election_reset_ms = 0;  /* 强制选举超时 */
    pthread_mutex_unlock(&srv->lock);
}

void raft_test_receive_heartbeat(RaftServer_t *srv, uint64_t from_node,
                                 uint64_t term) {
    if (!srv) return;
    pthread_mutex_lock(&srv->lock);
    if (term >= srv->current_term) {
        if (term > srv->current_term) {
            srv->current_term = term;
            srv->voted_for = RAFT_NO_VOTE;
        }
        if (srv->role == RAFT_ROLE_CANDIDATE) {
            srv->role = RAFT_ROLE_FOLLOWER;
        }
        srv->leader_id = from_node;
        srv->last_election_reset_ms = srv->use_virtual_clock ? srv->virtual_now_ms : now_ms();
    }
    pthread_mutex_unlock(&srv->lock);
}

/* ============================================================
 * P1 RPC 处理（多节点 + 日志复制）
 * ============================================================ */

RaftRequestVoteResult_t raft_handle_request_vote(RaftServer_t *srv,
                                                const RaftRequestVoteArgs_t *args) {
    RaftRequestVoteResult_t result = {0, false};
    if (!srv || !args) return result;

    pthread_mutex_lock(&srv->lock);
    result.term = srv->current_term;

    /* Term 检查：候选者 term < 自己 → reject */
    if (args->term < srv->current_term) {
        pthread_mutex_unlock(&srv->lock);
        return result;
    }

    /* Term 更大 → 更新 + step down */
    if (args->term > srv->current_term) {
        srv->current_term = args->term;
        srv->voted_for = RAFT_NO_VOTE;
        if (srv->role == RAFT_ROLE_LEADER || srv->role == RAFT_ROLE_CANDIDATE) {
            srv->role = RAFT_ROLE_FOLLOWER;
        }
    }

    /* 投票条件：未投票 OR 已投该候选者 */
    bool can_vote = (srv->voted_for == RAFT_NO_VOTE || srv->voted_for == args->candidate_id);

    /* 日志 up-to-date 检查 */
    bool log_ok = false;
    if (srv->log_size == 0) {
        log_ok = true;
    } else {
        uint64_t my_last_idx = (uint64_t)srv->log_size;
        uint64_t my_last_term = srv->log[srv->log_size - 1].term;
        if (args->last_log_term > my_last_term ||
            (args->last_log_term == my_last_term && args->last_log_index >= my_last_idx)) {
            log_ok = true;
        }
    }

    if (can_vote && log_ok && args->term >= srv->current_term) {
        srv->voted_for = args->candidate_id;
        srv->last_election_reset_ms = srv->use_virtual_clock ? srv->virtual_now_ms : now_ms();
        result.vote_granted = true;
        result.term = srv->current_term;
    } else {
        result.vote_granted = false;
        result.term = srv->current_term;
    }
    pthread_mutex_unlock(&srv->lock);
    return result;
}

RaftAppendEntriesResult_t raft_handle_append_entries(RaftServer_t *srv,
                                                    const RaftAppendEntriesArgs_t *args) {
    RaftAppendEntriesResult_t result = {0, false, 0};
    if (!srv || !args) return result;

    pthread_mutex_lock(&srv->lock);
    result.term = srv->current_term;

    /* Term 检查 */
    if (args->term < srv->current_term) {
        pthread_mutex_unlock(&srv->lock);
        return result;
    }

    /* Term 更大 → update + step down to follower */
    if (args->term > srv->current_term) {
        srv->current_term = args->term;
        srv->voted_for = RAFT_NO_VOTE;
    }
    if (srv->role == RAFT_ROLE_CANDIDATE || srv->role == RAFT_ROLE_LEADER) {
        srv->role = RAFT_ROLE_FOLLOWER;
    }
    srv->leader_id = args->leader_id;
    srv->last_election_reset_ms = srv->use_virtual_clock ? srv->virtual_now_ms : now_ms();

    /* prev_log 检查（全局索引 → 物理偏移转换） */
    if (args->prev_log_index > 0) {
        uint64_t last_global = srv->first_log_index + (uint64_t)srv->log_size - 1;
        if (srv->log_size == 0 || args->prev_log_index > last_global) {
            pthread_mutex_unlock(&srv->lock);
            return result;
        }
        if (args->prev_log_index < srv->first_log_index) {
            /* caller 需要先 install snapshot */
            pthread_mutex_unlock(&srv->lock);
            return result;
        }
        size_t phys_prev = (size_t)(args->prev_log_index - srv->first_log_index);
        if (srv->log[phys_prev].term != args->prev_log_term) {
            /* 删 conflict entries */
            for (size_t i = phys_prev + 1; i < srv->log_size; i++) {
                free(srv->log[i].data);
            }
            srv->log_size = phys_prev + 1;
            pthread_mutex_unlock(&srv->lock);
            return result;
        }
    }

    /* Append 新 entries（全局索引 → 物理偏移转换） */
    size_t start_phys = 0;
    if (srv->log_size > 0 && args->prev_log_index >= srv->first_log_index) {
        start_phys = (size_t)(args->prev_log_index - srv->first_log_index) + 1;
    }
    for (size_t i = 0; i < args->entry_count; i++) {
        size_t phys = start_phys + i;
        if (phys < srv->log_size) {
            /* 覆盖冲突 */
            free(srv->log[phys].data);
            srv->log[phys] = args->entries[i];
            /* 深拷贝 data */
            if (args->entries[i].data_size > 0 && args->entries[i].data) {
                void *dup = malloc(args->entries[i].data_size);
                if (!dup) {
                    pthread_mutex_unlock(&srv->lock);
                    return result;
                }
                memcpy(dup, args->entries[i].data, args->entries[i].data_size);
                srv->log[phys].data = dup;
            }
        } else {
            if (log_append(srv, args->entries[i].term, args->entries[i].data,
                            args->entries[i].data_size) != 0) {
                pthread_mutex_unlock(&srv->lock);
                return result;
            }
        }
    }
    result.success = true;

    /* 更新 commit_index */
    if (args->leader_commit > srv->commit_index) {
        uint64_t last_new = (uint64_t)args->prev_log_index + args->entry_count;
        srv->commit_index = args->leader_commit < last_new
                              ? args->leader_commit : last_new;
        if (srv->commit_index > srv->last_applied) {
            srv->last_applied = srv->commit_index;
        }
    }
    result.match_index = (uint64_t)args->prev_log_index + args->entry_count;
    pthread_mutex_unlock(&srv->lock);
    return result;
}

void raft_advance_commit_index(RaftServer_t *srv, uint64_t new_commit) {
    if (!srv) return;
    pthread_mutex_lock(&srv->lock);
    if (new_commit > srv->commit_index) {
        if (new_commit > (uint64_t)srv->log_size) {
            new_commit = (uint64_t)srv->log_size;
        }
        srv->commit_index = new_commit;
        if (srv->last_applied < srv->commit_index) {
            srv->last_applied = srv->commit_index;
        }
    }
    pthread_mutex_unlock(&srv->lock);
}

uint64_t raft_server_get_commit_index(const RaftServer_t *srv) {
    return raft_get_commit_index(srv);
}

size_t raft_server_get_log(const RaftServer_t *srv,
                           uint64_t start_index,
                           uint64_t *out_term,
                           const RaftLogEntry_t **out_entries) {
    if (!srv || !out_entries) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    if (start_index == 0 || start_index > (uint64_t)srv->log_size) {
        pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
        *out_entries = NULL;
        if (out_term) *out_term = 0;
        return 0;
    }
    size_t cnt = srv->log_size - start_index + 1;
    *out_entries = &srv->log[start_index - 1];
    if (out_term) *out_term = srv->log[0].term;  /* for completeness */
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return cnt;
}

/* ============================================================
 * P5 持久化实现
 * ============================================================ */

#define RAFT_STATE_MAGIC "RAFTv0.1"

static int write_u64(FILE *fp, uint64_t v) {
    return fwrite(&v, sizeof(v), 1, fp) == 1 ? 0 : -1;
}
static int read_u64(FILE *fp, uint64_t *v) {
    return fread(v, sizeof(*v), 1, fp) == 1 ? 0 : -1;
}

int raft_server_save_state(RaftServer_t *srv) {
    if (!srv || !srv->state_path) return -1;

    pthread_mutex_lock(&srv->save_lock);
    /* 复制快照（在 srv->lock 外持久化以减少持锁时间） */
    pthread_mutex_lock(&srv->lock);
    uint64_t current_term = srv->current_term;
    uint64_t voted_for = srv->voted_for;
    size_t log_size = srv->log_size;
    RaftLogEntry_t *log_snapshot = (RaftLogEntry_t *)malloc(sizeof(RaftLogEntry_t) * (log_size + 1));
    if (!log_snapshot) {
        pthread_mutex_unlock(&srv->lock);
        pthread_mutex_unlock(&srv->save_lock);
        return -1;
    }
    for (size_t i = 0; i < log_size; i++) {
        log_snapshot[i] = srv->log[i];
        if (log_snapshot[i].data_size > 0) {
            log_snapshot[i].data = malloc(log_snapshot[i].data_size);
            if (!log_snapshot[i].data) {
                for (size_t j = 0; j < i; j++) free(log_snapshot[j].data);
                free(log_snapshot);
                pthread_mutex_unlock(&srv->lock);
                pthread_mutex_unlock(&srv->save_lock);
                return -1;
            }
            memcpy(log_snapshot[i].data, srv->log[i].data, log_snapshot[i].data_size);
        }
    }
    pthread_mutex_unlock(&srv->lock);

    /* 写到临时文件，再 rename atomic */
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", srv->state_path);

    FILE *fp = fopen(tmp_path, "wb");
    int rc = -1;
    if (fp) {
        if (write_u64(fp, *(uint64_t *)RAFT_STATE_MAGIC) == 0 &&
            write_u64(fp, current_term) == 0 &&
            write_u64(fp, voted_for) == 0 &&
            write_u64(fp, log_size) == 0) {
            rc = 0;
            for (size_t i = 0; i < log_size; i++) {
                if (write_u64(fp, log_snapshot[i].term) != 0) { rc = -1; break; }
                if (write_u64(fp, log_snapshot[i].data_size) != 0) { rc = -1; break; }
                if (log_snapshot[i].data_size > 0) {
                    if (fwrite(log_snapshot[i].data, log_snapshot[i].data_size, 1, fp) != 1) { rc = -1; break; }
                }
            }
        }
        fclose(fp);
        if (rc == 0) {
            /* atomic rename: Windows 不允许 overwrite，先删 target */
            unlink(srv->state_path);
            if (rename(tmp_path, srv->state_path) != 0) {
                rc = -1;
            }
        } else {
            unlink(tmp_path);
        }
    }

    for (size_t i = 0; i < log_size; i++) free(log_snapshot[i].data);
    free(log_snapshot);
    pthread_mutex_unlock(&srv->save_lock);
    return rc;
}

int raft_server_load_state(RaftServer_t *srv, const char *path) {
    if (!srv || !path) return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    uint64_t magic;
    uint64_t current_term, voted_for, log_size;
    int rc = -1;

    if (read_u64(fp, &magic) == 0 &&
        magic == *(uint64_t *)RAFT_STATE_MAGIC &&
        read_u64(fp, &current_term) == 0 &&
        read_u64(fp, &voted_for) == 0 &&
        read_u64(fp, &log_size) == 0 && log_size < 1024) {
        pthread_mutex_lock(&srv->lock);
        srv->current_term = current_term;
        srv->voted_for = voted_for;
        /* 初始 commit_index 是从 log 末端读取（重建时 = log_size） */
        srv->commit_index = log_size;
        srv->last_applied = log_size;
        /* 清空现有 log */
        for (size_t i = 0; i < srv->log_size; i++) free(srv->log[i].data);
        srv->log_size = 0;

        size_t cap = log_size > srv->log_capacity ? (log_size + 16) : srv->log_capacity;
        if (cap > srv->log_capacity) {
            free(srv->log);
            srv->log = (RaftLogEntry_t *)calloc(cap, sizeof(RaftLogEntry_t));
            srv->log_capacity = cap;
        }

        rc = 0;
        for (size_t i = 0; i < log_size; i++) {
            uint64_t term, data_size;
            if (read_u64(fp, &term) != 0 || read_u64(fp, &data_size) != 0) { rc = -1; break; }
            srv->log[i].term = term;
            srv->log[i].index = (uint64_t)i + 1;
            srv->log[i].data_size = data_size;
            if (data_size > 0) {
                srv->log[i].data = malloc(data_size);
                if (!srv->log[i].data || fread(srv->log[i].data, data_size, 1, fp) != 1) { rc = -1; break; }
            }
            srv->log_size++;
        }
        pthread_mutex_unlock(&srv->lock);
    }
    fclose(fp);
    return rc;
}

void raft_test_reset_state(RaftServer_t *srv) {
    if (!srv) return;
    pthread_mutex_lock(&srv->lock);
    srv->current_term = 0;
    srv->voted_for = RAFT_NO_VOTE;
    for (size_t i = 0; i < srv->log_size; i++) free(srv->log[i].data);
    srv->log_size = 0;
    pthread_mutex_unlock(&srv->lock);
}

void raft_test_become_leader(RaftServer_t *srv) {
    if (!srv) return;
    pthread_mutex_lock(&srv->lock);
    if (srv->role == RAFT_ROLE_CANDIDATE || srv->role == RAFT_ROLE_FOLLOWER) {
        srv->role = RAFT_ROLE_LEADER;
        srv->leader_id = srv->config.node_id;
    }
    pthread_mutex_unlock(&srv->lock);
}

/* ============================================================
 * P7 Snapshot + Log Compaction
 * ============================================================ */

int raft_take_snapshot(RaftServer_t *srv,
                       const void *snapshot_data, size_t data_size,
                       uint64_t last_included_index) {
    if (!srv) return -1;

    pthread_mutex_lock(&srv->lock);

    /* 不能快照尚未提交的日志 */
    if (last_included_index > srv->commit_index) {
        pthread_mutex_unlock(&srv->lock);
        return -1;
    }
    if (last_included_index == 0) {
        pthread_mutex_unlock(&srv->lock);
        return -1;
    }

    /* 释放旧 snapshot */
    free(srv->snapshot_data);
    srv->snapshot_data = NULL;
    srv->snapshot_size = 0;

    /* 深拷贝应用层 snapshot_data */
    if (data_size > 0 && snapshot_data) {
        srv->snapshot_data = malloc(data_size);
        if (!srv->snapshot_data) {
            pthread_mutex_unlock(&srv->lock);
            return -1;
        }
        memcpy(srv->snapshot_data, snapshot_data, data_size);
        srv->snapshot_size = data_size;
    }

    /* 记录快照元信息 */
    uint64_t last_term = 0;
    uint64_t log_offset = last_included_index - srv->first_log_index + 1; /* 1-based */
    if (log_offset > 0 && log_offset <= (uint64_t)srv->log_size) {
        last_term = srv->log[log_offset - 1].term;
    }
    srv->has_snapshot = true;
    srv->snapshot_last_index = last_included_index;
    srv->snapshot_last_term = last_term;

    /* === 日志压缩：丢弃 last_included_index 之前的日志 === */
    uint64_t first_keep_idx = last_included_index + 1;
    if (first_keep_idx > srv->first_log_index) {
        uint64_t shift = first_keep_idx - srv->first_log_index;
        if (shift >= (uint64_t)srv->log_size) {
            /* 全删 */
            for (size_t i = 0; i < srv->log_size; i++) free(srv->log[i].data);
            srv->log_size = 0;
        } else {
            /* 释放要丢弃的条目 */
            for (size_t i = 0; i < (size_t)shift && i < srv->log_size; i++) {
                free(srv->log[i].data);
            }
            /* 前移剩余条目 */
            size_t remaining = srv->log_size - (size_t)shift;
            memmove(srv->log, srv->log + shift, remaining * sizeof(RaftLogEntry_t));
            srv->log_size = remaining;
        }
        srv->first_log_index = first_keep_idx;
    }

    srv->dirty = true;
    pthread_mutex_unlock(&srv->lock);
    return 0;
}

int raft_install_snapshot(RaftServer_t *srv,
                          const void *snapshot_data, size_t data_size,
                          uint64_t last_included_index,
                          uint64_t last_included_term) {
    if (!srv) return -1;

    pthread_mutex_lock(&srv->lock);

    /* 清空现有日志 */
    for (size_t i = 0; i < srv->log_size; i++) free(srv->log[i].data);
    srv->log_size = 0;

    /* 释放旧 snapshot */
    free(srv->snapshot_data);
    srv->snapshot_data = NULL;
    srv->snapshot_size = 0;

    /* 深拷贝 snapshot */
    if (data_size > 0 && snapshot_data) {
        srv->snapshot_data = malloc(data_size);
        if (!srv->snapshot_data) {
            pthread_mutex_unlock(&srv->lock);
            return -1;
        }
        memcpy(srv->snapshot_data, snapshot_data, data_size);
        srv->snapshot_size = data_size;
    }

    srv->has_snapshot = true;
    srv->snapshot_last_index = last_included_index;
    srv->snapshot_last_term = last_included_term;

    /* 安装后重置：commit/applied 到 snapshot 末端 */
    if (last_included_index > srv->commit_index) {
        srv->commit_index = last_included_index;
    }
    if (last_included_index > srv->last_applied) {
        srv->last_applied = last_included_index;
    }
    srv->first_log_index = last_included_index + 1;

    srv->dirty = true;
    pthread_mutex_unlock(&srv->lock);
    return 0;
}

int raft_get_snapshot(const RaftServer_t *srv,
                      void *buf, size_t cap, size_t *out_size,
                      uint64_t *out_last_included_index,
                      uint64_t *out_last_included_term) {
    if (!srv) return -1;

    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);

    if (!srv->has_snapshot) {
        pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
        return -1;
    }

    if (out_size) *out_size = srv->snapshot_size;
    if (out_last_included_index) *out_last_included_index = srv->snapshot_last_index;
    if (out_last_included_term) *out_last_included_term = srv->snapshot_last_term;

    if (buf && cap > 0 && srv->snapshot_data && srv->snapshot_size > 0) {
        size_t copy = cap < srv->snapshot_size ? cap : srv->snapshot_size;
        memcpy(buf, srv->snapshot_data, copy);
    }

    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return 0;
}

bool raft_has_snapshot(const RaftServer_t *srv) {
    if (!srv) return false;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    bool hs = srv->has_snapshot;
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return hs;
}

uint64_t raft_get_snapshot_last_index(const RaftServer_t *srv) {
    if (!srv) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    uint64_t idx = srv->snapshot_last_index;
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return idx;
}

uint64_t raft_get_snapshot_last_term(const RaftServer_t *srv) {
    if (!srv) return 0;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    uint64_t t = srv->snapshot_last_term;
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return t;
}

uint64_t raft_get_first_log_index(const RaftServer_t *srv) {
    if (!srv) return 1;
    pthread_mutex_lock((pthread_mutex_t *)&srv->lock);
    uint64_t idx = srv->first_log_index;
    pthread_mutex_unlock((pthread_mutex_t *)&srv->lock);
    return idx;
}
