/**
 * @file amapi.c
 * @brief Index AM 注册表实现
 */
#include "db/access/amapi.h"
#include <string.h>

/* ============================================================
 * AM 注册表（固定大小数组，无动态分配）
 * ============================================================ */

#define MAX_AM_REGISTRY 16

static const IndexAmRoutine *g_am_registry[MAX_AM_REGISTRY];
static int g_am_count = 0;
static bool g_am_initialized = false;

/* ============================================================
 * 公共 API
 * ============================================================ */

void index_am_init(void)
{
    if (g_am_initialized) {
        return;
    }
    g_am_count = 0;
    g_am_initialized = true;
}

int index_am_register(const char *am_name, const IndexAmRoutine *routine)
{
    if (!g_am_initialized) {
        index_am_init();
    }
    if (!am_name || !routine || g_am_count >= MAX_AM_REGISTRY) {
        return -1;
    }
    /* 检查是否已注册同名 AM */
    for (int i = 0; i < g_am_count; i++) {
        if (g_am_registry[i]->am_name &&
            strcmp(g_am_registry[i]->am_name, am_name) == 0) {
            return -1; /* 已存在 */
        }
    }
    g_am_registry[g_am_count++] = routine;
    return 0;
}

const IndexAmRoutine *index_am_get(const char *am_name)
{
    if (!g_am_initialized || !am_name) {
        return NULL;
    }
    for (int i = 0; i < g_am_count; i++) {
        if (g_am_registry[i]->am_name &&
            strcmp(g_am_registry[i]->am_name, am_name) == 0) {
            return g_am_registry[i];
        }
    }
    return NULL;
}