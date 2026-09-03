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
    int64_t watermark;       /* 持久化水位：恢复后保证物理绝不倒退（默认 0 表示无水位） */
};

/* 物理时间戳绝对单调：本地物理时钟比已分配戳小则沿用已分配戳（不倒退）；
   恢复的水位再兜底一层：偶发时钟 / 故障重启回退时，至少回到水位对应物理。 */
static int64_t next_physical(tso_oracle_t *o) {
    int64_t now = 0;
    if (g_clock(&now) != 0) now = o->last_physical;
    if (now < o->last_physical) now = o->last_physical;
    if (o->watermark) {
        int64_t wm_phys = o->watermark >> TSO_LOGICAL_BITS;
        if (wm_phys > now) now = wm_phys;      /* 水位兜底：绝不再退 */
    }
    return now;
}

int tso_oracle_init(tso_oracle_t **out) {
    tso_oracle_t *o = calloc(1, sizeof(*o));
    if (!o) return -1;
    pthread_mutex_init(&o->mu, NULL);
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
    if (count > TSO_LOGICAL_MASK + 1) count = TSO_LOGICAL_MASK;
    pthread_mutex_lock(&o->mu);
    int64_t base = next_physical(o);
    uint32_t start_logical = o->last_logical;
    int64_t raw = ((int64_t)base << TSO_LOGICAL_BITS) | start_logical;
    uint32_t slots_left = (uint32_t)(TSO_LOGICAL_MASK + 1) - start_logical;
    int64_t end;
    uint32_t new_logical;
    if (count < (int)slots_left) {
        /* 不跨物理：批全部落于当前物理内，游标同物理续进 */
        end = ((int64_t)base << TSO_LOGICAL_BITS) | (start_logical + (uint32_t)count - 1);
        new_logical = start_logical + (uint32_t)count;
    } else {
        /* 批占满当前物理剩余槽(slots_left)，必要时余数跨入下一物理 */
        int remainder = count - (int)slots_left;
        base += 1;
        if (remainder == 0) {
            /* 恰好占满：终点为当前物理 MASK，下一物理 logical 归 0 且物理进位 */
            end = ((int64_t)(base - 1) << TSO_LOGICAL_BITS) | TSO_LOGICAL_MASK;
            new_logical = 0;
        } else {
            /* 多余的 remainder 落在 base+1 物理 */
            end = ((int64_t)base << TSO_LOGICAL_BITS) | ((uint32_t)remainder - 1);
            new_logical = (uint32_t)remainder;
        }
    }
    o->last_physical = base;
    o->last_logical = new_logical;
    *out_start = raw;
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

/* 峰值 = 当前已分配游标（last_physical<<BITS | last_logical），
   即"下一个待分配戳"，作为水位上界持久化。 */
int64_t tso_oracle_peak(const tso_oracle_t *o) {
    if (!o) return 0;
    return ((int64_t)o->last_physical << TSO_LOGICAL_BITS) | o->last_logical;
}

/* 恢复：把持久化水位插回本实例。提高 watermark 上界；
   若水位对应物理更高，则同步推进 last_physical 与 last_logical 游标，
   使重启后首条分配严格高于旧峰值，绝不回退。 */
int tso_oracle_insert_watermark(tso_oracle_t *o, int64_t ts) {
    if (!o || ts <= 0) return -1;
    if (ts > o->watermark) o->watermark = ts;
    int64_t phys = ts >> TSO_LOGICAL_BITS;
    int64_t logi = ts & TSO_LOGICAL_MASK;
    if (phys > o->last_physical ||
        (phys == o->last_physical && logi > o->last_logical)) {
        o->last_physical = phys;
        o->last_logical  = (uint32_t)logi;
    }
    return 0;
}