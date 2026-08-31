/* tso_codec.c —— TSO 编解码与单调分配（HLC Oracle） */
#include "distributed/tso.h"
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

int64_t tso_encode(const tso_ts_t *ts) {
    return (int64_t)(ts->physical_ms << TSO_LOGICAL_BITS) | ts->logical;
}

void tso_decode(int64_t raw, tso_ts_t *out) {
    out->physical_ms = raw >> TSO_LOGICAL_BITS;
    out->logical = (uint32_t)(raw & TSO_LOGICAL_MASK);
}

/* 默认物理时钟：gettimeofday 毫秒 */
static int default_clock(int64_t *ms) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *ms = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return 0;
}
static tso_clock_source_t g_clock = default_clock;

void tso_set_clock_source(tso_clock_source_t src) {
    if (src) g_clock = src;
}

struct tso_oracle {
    pthread_mutex_t mu;
    int64_t last_physical;   /* 已分配的最大物理时间 */
    uint32_t last_logical;   /* 相应逻辑计数 */
};

/* 物理时间戳绝对单调：本地物理时钟比已分配戳小则沿用已分配戳（不倒退） */
static int64_t next_physical(tso_oracle_t *o) {
    int64_t now = 0;
    if (g_clock(&now) != 0) now = o->last_physical;
    if (now < o->last_physical) now = o->last_physical;
    return now;
}

int tso_oracle_init(tso_oracle_t **out) {
    tso_oracle_t *o = calloc(1, sizeof(*o));
    if (!o) return -1;
    pthread_mutex_init(&o->mu, NULL);
    o->last_physical = next_physical(o) ? 0 : 0;   /* 初始以时钟起 */
    int64_t now = 0;
    if (g_clock(&now) == 0) o->last_physical = now;
    *out = o;
    return 0;
}

void tso_oracle_destroy(tso_oracle_t *o) {
    if (!o) return;
    pthread_mutex_destroy(&o->mu);
    free(o);
}

int tso_alloc(tso_oracle_t *o, int count, int64_t *out_start, int64_t *out_end) {
    if (!o || count <= 0) return -1;
    if (count > TSO_LOGICAL_MASK + 1) count = TSO_LOGICAL_MASK;  /* 逻辑位容量上限 */
    pthread_mutex_lock(&o->mu);
    int64_t base = next_physical(o);
    int64_t raw = ((int64_t)base << TSO_LOGICAL_BITS) | o->last_logical;
    *out_start = raw;                                    /* 从当前逻辑续起 */
    for (int i = 0; i < count; ++i) {
        /* 逻辑溢出：切到下一个物理时间，逻辑归 0 */
        if (o->last_logical + (uint32_t)(i + 1) > TSO_LOGICAL_MASK) {
            base += 1;
            o->last_physical = base;
            o->last_logical = 0;
        }
    }
    /* 校正区间终点并推进状态 */
    int64_t end = ((int64_t)base << TSO_LOGICAL_BITS) |
                  (o->last_logical + (uint32_t)count - 1);
    if (end < raw) end = raw; /* 防御：保证区间终点不小于起点（真实物理时钟下原 mask 判定不成立） */
    o->last_physical = base;
    o->last_logical = (uint32_t)((o->last_logical + (uint32_t)count) & TSO_LOGICAL_MASK);
    *out_end = end;
    pthread_mutex_unlock(&o->mu);
    return 0;
}

int64_t tso_oracle_now(tso_oracle_t *o) {
    int64_t s = 0, e = 0;
    if (!o) return 0;
    if (tso_alloc(o, 1, &s, &e) != 0) return 0;
    return s;
}