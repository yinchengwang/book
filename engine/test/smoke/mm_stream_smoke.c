/**
 * @file mm_stream_smoke.c
 * @brief stream 模态独立冒烟测试（Task 11 接入）
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include "mm_smoke_assert.h"
#include <assert.h>
#include <stdio.h>

int main(void) {

    assert(mm_storage_init("test_data/smoke_stream") == 0);

    /* profile 感知断言：注册槽位集合必须恰好等于本 profile 期望掩码，
     * 且本模态引擎已注册（冒烟的核心命题）。 */
    mm_smoke_assert_engine_set(MODEL_STREAM, 1);
    assert(mm_storage_shutdown() == 0);
    printf("mm_stream_smoke OK\n");
    return 0;
}
