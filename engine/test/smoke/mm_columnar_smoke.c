/**
 * @file mm_columnar_smoke.c
 * @brief columnar 模态独立冒烟测试（Task 11 接入）
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_columnar") == 0);
    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m)
        if (storage_get_engine((DataModel)m) != NULL) ++n;
    assert(n == 1);
    assert(storage_get_engine(MODEL_COLUMNAR) != NULL);
    assert(mm_storage_shutdown() == 0);
    printf("mm_columnar_smoke OK\n");
    return 0;
}
