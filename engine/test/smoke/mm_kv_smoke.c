/**
 * @file mm_kv_smoke.c
 * @brief KV 模态独立冒烟测试
 *
 * 链接期证明：本目标只链 mm_base + mm_storage_api + mm_kv，
 * 不链 db_storage 或任何其他模态库。
 * 运行期证明：engine_registry_init 后注册表中只有 KV 一个引擎。
 */
#include "db/mm_engine_register.h"
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_kv") == 0);

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            ++n;
        }
    }

    /* 独立性证明：kv profile 下只应有一个引擎（10 号保留空槽恒为 NULL） */
    assert(n == 1);
    assert(storage_get_engine(MODEL_KV) != NULL);
    assert(storage_get_engine(MODEL_VECTOR) == NULL);
    assert(storage_get_engine(MODEL_GRAPH) == NULL);

    assert(mm_storage_shutdown() == 0);
    printf("mm_kv_smoke OK\n");
    return 0;
}
