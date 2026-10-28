/**
 * @file mm_yang_smoke.c
 * @brief 树（YANG）模态独立冒烟测试
 *
 * 链接期证明：只链 mm_base + mm_storage_api + mm_yang
 * （tree profile，库名与模态名不一一对应：tree -> mm_yang）。
 * 运行期证明：tree profile 下只注册 TREE 一个引擎。
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_tree") == 0);

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            ++n;
        }
    }
    assert(n == 1);
    assert(storage_get_engine(MODEL_TREE) != NULL);
    assert(storage_get_engine(MODEL_KV) == NULL);

    assert(mm_storage_shutdown() == 0);
    printf("mm_yang_smoke OK\n");
    return 0;
}
