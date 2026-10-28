/**
 * @file mm_doc_smoke.c
 * @brief 文档模态独立冒烟测试
 *
 * 链接期证明：只链 mm_base + mm_storage_api + mm_doc（mm_vector 经
 * mm_doc PUBLIC 传递，doc 白名单依赖 vector 的 hnsw）。
 * 运行期证明：doc profile 闭包 = doc + vector；SPARSE 不注册引擎，
 * 故注册表计数为 2（DOCUMENT + VECTOR）。
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_doc") == 0);

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            ++n;
        }
    }
    assert(n == 2);
    assert(storage_get_engine(MODEL_DOCUMENT) != NULL);
    assert(storage_get_engine(MODEL_VECTOR) != NULL);
    assert(storage_get_engine(MODEL_KV) == NULL);

    assert(mm_storage_shutdown() == 0);
    printf("mm_doc_smoke OK\n");
    return 0;
}
