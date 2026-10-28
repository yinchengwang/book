/**
 * @file mm_ts_smoke.c
 * @brief 时序模态独立冒烟测试
 *
 * 链接期证明：只链 mm_base + mm_storage_api + mm_ts。
 * 运行期证明：ts profile 下只注册 TIMESERIES 一个引擎。
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_ts") == 0);

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            ++n;
        }
    }
    assert(n == 1);
    assert(storage_get_engine(MODEL_TIMESERIES) != NULL);
    assert(storage_get_engine(MODEL_KV) == NULL);

    assert(mm_storage_shutdown() == 0);
    printf("mm_ts_smoke OK\n");
    return 0;
}
