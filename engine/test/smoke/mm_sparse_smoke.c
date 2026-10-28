/**
 * @file mm_sparse_smoke.c
 * @brief 稀疏检索模态独立冒烟测试
 *
 * 链接期证明：只链 mm_base + mm_storage_api + mm_sparse（+ mm_vector，
 * 白名单依赖闭包 sparse -> vector）。
 * 运行期说明：SPARSE 不实现 storage_ops_t（无引擎注册，见
 * engine_registry.c），因此注册表中只有 VECTOR 一个引擎；
 * sparse 自身以 bm25/sparse_vector 检索 API 形式提供，能链接即证明其独立可用。
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_sparse") == 0);

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            ++n;
        }
    }
    /* sparse profile 闭包 = vector + sparse；SPARSE 不注册 storage 引擎，
     * 因此引擎计数为 1（VECTOR），注意不要误设为 2 */
    assert(n == 1);
    assert(storage_get_engine(MODEL_VECTOR) != NULL);

    assert(mm_storage_shutdown() == 0);
    printf("mm_sparse_smoke OK\n");
    return 0;
}
