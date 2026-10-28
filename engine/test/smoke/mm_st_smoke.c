/**
 * @file mm_st_smoke.c
 * @brief 时空模态独立冒烟测试
 *
 * 链接期证明：只链 mm_base + mm_storage_api + mm_st（mm_spatial 经
 * mm_st PUBLIC 传递，st 白名单依赖 spatial 的几何类型与引擎）。
 * 运行期证明：st profile 闭包 = st + spatial，共 2 个引擎。
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_st") == 0);

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            ++n;
        }
    }
    assert(n == 2);
    assert(storage_get_engine(MODEL_SPATIOTEMPORAL) != NULL);
    assert(storage_get_engine(MODEL_SPATIAL) != NULL);
    assert(storage_get_engine(MODEL_KV) == NULL);

    assert(mm_storage_shutdown() == 0);
    printf("mm_st_smoke OK\n");
    return 0;
}
