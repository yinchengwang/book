/**
 * @file parallel.c
 * @brief 并行执行框架实现
 */

#include "db/sql/parallel.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ========================================================================
 * ParallelContext API
 * ======================================================================== */

ParallelContext *CreateParallelContext(int nworkers) {
    ParallelContext *pcxt;

    if (nworkers <= 0) {
        return NULL;
    }

    pcxt = (ParallelContext *)calloc(1, sizeof(ParallelContext));
    if (!pcxt) {
        return NULL;
    }

    pcxt->type = T_ParallelContext;
    pcxt->nworkers = nworkers;
    pcxt->nworkers_to_launch = nworkers;
    pcxt->nworkers_launched = 0;

    /* 创建协调器 */
    pcxt->coordinator = (ParallelCoordinator *)calloc(1, sizeof(ParallelCoordinator));
    if (!pcxt->coordinator) {
        free(pcxt);
        return NULL;
    }

    pcxt->coordinator->type = T_ParallelCoordinator;
    pcxt->coordinator->nworkers = nworkers;
    pcxt->coordinator->nworkers_attached = 0;
    pcxt->coordinator->finished = false;

    /* DSM 句柄（简化实现：后续按需分配） */
    pcxt->dsm_handle = NULL;

    return pcxt;
}

void DestroyParallelContext(ParallelContext *pcxt) {
    if (!pcxt) {
        return;
    }

    /* 等待所有 worker 完成 */
    WaitForParallelWorkers(pcxt);

    /* 释放协调器 */
    if (pcxt->coordinator) {
        free(pcxt->coordinator);
    }

    /* 释放 DSM */
    if (pcxt->dsm_handle) {
        free(pcxt->dsm_handle);
    }

    free(pcxt);
}

void *ParallelAlloc(ParallelContext *pcxt, size_t size) {
    if (!pcxt) {
        return NULL;
    }

    /* 简化实现：使用 calloc */
    return calloc(1, size);
}

void *ParallelGetWorkerAddr(ParallelContext *pcxt, void *addr, int worker_id) {
    /* 简化实现：所有 worker 共享同一地址空间 */
    (void)pcxt;
    (void)worker_id;
    return addr;
}

int LaunchParallelWorkers(ParallelContext *pcxt) {
    if (!pcxt) {
        return 0;
    }

    /* 简化实现：记录启动数量 */
    pcxt->nworkers_launched = pcxt->nworkers_to_launch;

    return pcxt->nworkers_launched;
}

void WaitForParallelWorkers(ParallelContext *pcxt) {
    if (!pcxt) {
        return;
    }

    /* 简化实现：标记完成 */
    if (pcxt->coordinator) {
        pcxt->coordinator->finished = true;
    }
}

/* ========================================================================
 * ParallelCoordinator API
 * ======================================================================== */

void ParallelCoordinatorSetFinished(ParallelCoordinator *coordinator) {
    if (coordinator) {
        coordinator->finished = true;
    }
}

bool ParallelCoordinatorIsFinished(ParallelCoordinator *coordinator) {
    return coordinator ? coordinator->finished : true;
}

void ParallelCoordinatorAttach(ParallelCoordinator *coordinator) {
    if (coordinator) {
        coordinator->nworkers_attached++;
    }
}

/* ========================================================================
 * ParallelScanState API
 * ======================================================================== */

ParallelScanState *CreateParallelScanState(void *rel, int total_pages, int nworkers) {
    ParallelScanState *pstate;

    if (total_pages <= 0 || nworkers <= 0) {
        return NULL;
    }

    pstate = (ParallelScanState *)calloc(1, sizeof(ParallelScanState));
    if (!pstate) {
        return NULL;
    }

    pstate->type = T_ParallelScanState;
    pstate->total_pages = total_pages;
    pstate->next_page = 0;
    pstate->pages_scanned = 0;
    pstate->workers_finished = 0;
    pstate->nworkers = nworkers;
    pstate->relation = rel;
    pstate->scan_flags = 0;

    return pstate;
}

void DestroyParallelScanState(ParallelScanState *pstate) {
    if (!pstate) {
        return;
    }
    free(pstate);
}

int ParallelScanNextPage(ParallelScanState *pstate) {
    if (!pstate) {
        return -1;
    }

    /* 原子获取下一个页面号 */
    int page;
#ifdef _WIN32
    page = InterlockedIncrement((volatile LONG *)&pstate->next_page) - 1;
#else
    page = __sync_fetch_and_add(&pstate->next_page, 1);
#endif

    if (page >= pstate->total_pages) {
        return -1;  /* 所有页面已分配 */
    }

#ifdef _WIN32
    InterlockedIncrement((volatile LONG *)&pstate->pages_scanned);
#else
    __sync_fetch_and_add(&pstate->pages_scanned, 1);
#endif

    return page;
}

void ParallelScanWorkerFinished(ParallelScanState *pstate) {
    if (!pstate) {
        return;
    }

#ifdef _WIN32
    InterlockedIncrement((volatile LONG *)&pstate->workers_finished);
#else
    __sync_fetch_and_add(&pstate->workers_finished, 1);
#endif
}

bool ParallelScanAllFinished(ParallelScanState *pstate) {
    if (!pstate) {
        return true;
    }
    return pstate->workers_finished >= pstate->nworkers;
}

/* ========================================================================
 * ParallelHash API
 * ======================================================================== */

ParallelHashJoinState *CreateParallelHash(int nparticipants, uint64_t nbuckets) {
    ParallelHashJoinState *state;

    if (nparticipants <= 0 || nbuckets == 0) {
        return NULL;
    }

    state = (ParallelHashJoinState *)calloc(1, sizeof(ParallelHashJoinState));
    if (!state) {
        return NULL;
    }

    state->type = T_ParallelHashJoinState;
    state->nbuckets = nbuckets;
    state->ntuples = 0;
    state->nparticipants = nparticipants;

    /* 分配桶数组 */
    state->buckets = calloc(nbuckets, sizeof(void *));
    if (!state->buckets) {
        free(state);
        return NULL;
    }

    state->chunks = NULL;
    state->hashtable = state;

    return state;
}

void DestroyParallelHash(ParallelHashJoinState *state) {
    if (!state) {
        return;
    }

    if (state->buckets) {
        /* 释放所有桶中的条目 */
        for (uint64_t i = 0; i < state->nbuckets; i++) {
            void *entry = ((void **)state->buckets)[i];
            while (entry) {
                void *next = *(void **)entry;
                free(entry);
                entry = next;
            }
        }
        free(state->buckets);
    }

    /* 释放 chunks */
    if (state->chunks) {
        void *chunk = state->chunks;
        while (chunk) {
            void *next = *(void **)chunk;
            free(chunk);
            chunk = next;
        }
    }

    free(state);
}

/**
 * @brief 插入键值对到并行哈希表
 *
 * @param state 并行哈希状态
 * @param key   键
 * @param value 值
 *
 * @note entry 布局：[0] = next, [1] = key, [2] = value
 */
void ParallelHashInsert(ParallelHashJoinState *state, void *key, void *value) {
    if (!state || !key) {
        return;
    }

    /* 计算桶索引 */
    uint64_t hash = (uint64_t)(uintptr_t)key % state->nbuckets;
    void **bucket = (void **)state->buckets + hash;

    /* 分配 entry：包含 next、key、value 三个指针 */
    void *entry = malloc(3 * sizeof(void *));
    if (!entry) {
        return;
    }

    *(void **)entry = *bucket;            /* next */
    *((void **)entry + 1) = key;         /* key */
    *((void **)entry + 2) = value;      /* value */
    *bucket = entry;

    state->ntuples++;
}

/**
 * @brief 在并行哈希表中查找键对应的值
 *
 * @param state 并行哈希状态
 * @param key   键
 *
 * @return 找到的值；未找到返回 NULL
 */
void *ParallelHashLookup(ParallelHashJoinState *state, void *key) {
    if (!state || !key) {
        return NULL;
    }

    uint64_t hash = (uint64_t)(uintptr_t)key % state->nbuckets;
    void *entry = ((void **)state->buckets)[hash];

    while (entry) {
        void *ekey = *((void **)entry + 1);
        if (ekey == key) {
            return *((void **)entry + 2);
        }
        entry = *(void **)entry;  /* 推进到下一个条目 */
    }

    return NULL;
}