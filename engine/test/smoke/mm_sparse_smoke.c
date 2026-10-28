/**
 * @file mm_sparse_smoke.c
 * @brief 稀疏检索模态独立冒烟测试
 *
 * 链接期证明：只链 mm_base + mm_storage_api + mm_sparse（+ mm_vector，
 * 白名单依赖闭包 sparse -> vector）。
 * 运行期证明：注册表槽位集合恰等于本 profile 期望掩码，且本模态引擎
 * 已注册（掩码由构建期按 profile 生成，见 cmake/mm_smoke_expect.h.in）。
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include "mm_smoke_assert.h"
#include <assert.h>
#include <stdio.h>

int main(void) {

    assert(mm_storage_init("test_data/smoke_sparse") == 0);

    /* profile 感知断言：注册槽位集合必须恰好等于本 profile 期望掩码，
     * 且本模态引擎已注册（冒烟的核心命题）。 */
    mm_smoke_assert_engine_set(MODEL_VECTOR, 1);

    /* sparse 不实现 storage_ops_t（无自身引擎槽位），只断言槽位集合
     * 等于本 profile 期望掩码；vector 由 sparse profile 的依赖闭包带入。 */

    assert(mm_storage_shutdown() == 0);
    printf("mm_sparse_smoke OK\n");
    return 0;
}
