/**
 * @file bgwriter.c
 * @brief 后台写进程实现
 */

#include "db/storage/buffer/bgwriter.h"
#include "db/buf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* ============================================================
 * 全局状态
 * ============================================================ */

static bool g_bgwriter_running = false;
static bool g_bgwriter_stop = false;
static pthread_t g_bgwriter_thread;
static int g_bgwriter_delay_ms = BGWRITER_DELAY_DEFAULT;
static int g_bgwriter_max_pages = BGWRITER_MAX_PAGES_DEFAULT;
static int g_bgwriter_next_victim = 0;
static uint64_t g_checkpoints_timed = 0;
static uint64_t g_checkpoints_req = 0;
static uint64_t g_checkpoints_written = 0;
static uint64_t g_bgwriter_writes = 0;
static uint64_t g_bgwriter_startups = 0;
static uint64_t g_bgwriter_shutdowns = 0;

/* ============================================================
 * 内部辅助
 * ============================================================ */

static void bgwriter_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static int bgwriter_write_dirty_pages(void) {
    if (!buf_initialized) return 0;
    int count = buf_flush_all();
    if (count > 0) {
        g_bgwriter_writes += (uint64_t)count;
    }
    return count;
}

static void *bgwriter_main(void *arg) {
    (void)arg;
    g_bgwriter_running = true;
    g_bgwriter_startups++;

    while (!g_bgwriter_stop) {
        bgwriter_sleep_ms(g_bgwriter_delay_ms);
        if (g_bgwriter_stop) break;
        bgwriter_write_dirty_pages();
    }

    g_bgwriter_running = false;
    g_bgwriter_shutdowns++;
    return NULL;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

int bgwriter_start(void) {
    if (g_bgwriter_running) return 0;
    g_bgwriter_stop = false;
    if (pthread_create(&g_bgwriter_thread, NULL, bgwriter_main, NULL) != 0) {
        return -1;
    }
    return 0;
}

void bgwriter_stop(void) {
    if (!g_bgwriter_running) return;
    g_bgwriter_stop = true;
    pthread_join(g_bgwriter_thread, NULL);
}

bool bgwriter_is_running(void) {
    return g_bgwriter_running;
}

int bgwriter_do_checkpoint(void) {
    g_checkpoints_req++;
    int count = buf_flush_all();
    g_checkpoints_written += (uint64_t)count;
    g_checkpoints_timed++;
    return count;
}

void bgwriter_set_delay(int delay_ms) {
    if (delay_ms < 10) delay_ms = 10;
    if (delay_ms > 60000) delay_ms = 60000;
    g_bgwriter_delay_ms = delay_ms;
}

int bgwriter_get_delay(void) {
    return g_bgwriter_delay_ms;
}

void bgwriter_set_max_pages(int max_pages) {
    if (max_pages < 0) max_pages = 0;
    if (max_pages > 1000) max_pages = 1000;
    g_bgwriter_max_pages = max_pages;
}

void bgwriter_get_stats(BgWriterStats *stats) {
    if (!stats) return;
    stats->checkpoints_timed = g_checkpoints_timed;
    stats->checkpoints_req = g_checkpoints_req;
    stats->checkpoints_written = g_checkpoints_written;
    stats->bgwriter_writes = g_bgwriter_writes;
    stats->bgwriter_startups = g_bgwriter_startups;
    stats->bgwriter_shutdowns = g_bgwriter_shutdowns;
}

void bgwriter_reset_stats(void) {
    g_checkpoints_timed = 0;
    g_checkpoints_req = 0;
    g_checkpoints_written = 0;
    g_bgwriter_writes = 0;
}
