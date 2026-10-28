/**
 * @file mm_kv_smoke.c
 * @brief KV 模态独立冒烟测试
 *
 * 链接期证明：本目标只链 mm_base + mm_storage_api + mm_kv，
 * 不链 db_storage 或任何其他模态库。
 * 运行期证明：注册表槽位集合恰等于本 profile 期望掩码，且本模态引擎
 * 已注册（掩码由构建期按 profile 生成，见 cmake/mm_smoke_expect.h.in）。
 */
#include "db/mm_engine_register.h"
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include "mm_smoke_assert.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_kv") == 0);

    /* profile 感知断言：注册槽位集合必须恰好等于本 profile 期望掩码，
     * 且本模态引擎已注册（冒烟的核心命题）。 */
    mm_smoke_assert_engine_set(MODEL_KV, 1);

    assert(mm_storage_shutdown() == 0);
    printf("mm_kv_smoke OK\n");
    return 0;
}
