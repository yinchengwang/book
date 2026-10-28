/**
 * @file mm_vector_smoke.c
 * @brief 向量模态独立冒烟测试
 *
 * 链接期证明：本目标只链 mm_base + mm_storage_api + mm_vector
 * （vector_index 子库经 mm_vector PUBLIC 传递），不链 db_storage/db_core。
 * 运行期证明：注册表槽位集合恰等于本 profile 期望掩码，且本模态引擎
 * 已注册（掩码由构建期按 profile 生成，见 cmake/mm_smoke_expect.h.in）。
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include "mm_smoke_assert.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_vector") == 0);

    /* profile 感知断言：注册槽位集合必须恰好等于本 profile 期望掩码，
     * 且本模态引擎已注册（冒烟的核心命题）。 */
    mm_smoke_assert_engine_set(MODEL_VECTOR, 1);

    const storage_ops_t *ops = storage_get_engine(MODEL_VECTOR);

    /* 真实写入：建集合 -> 打开 -> 插入一条 128 维向量记录 */
    assert(ops->table_create("smoke_v", NULL) == 0);
    void *rel = ops->table_open("smoke_v", ACCESS_MODE_WRITE);
    assert(rel != NULL);

    /* C0-3 旧格式：[uint64 id][int32 dim][128 x float] */
    enum { DIM = 128 };
    unsigned char rec[sizeof(uint64_t) + sizeof(int32_t) + DIM * sizeof(float)];
    {
        uint64_t id = 42;
        int32_t dim = DIM;
        memcpy(rec, &id, sizeof(id));
        memcpy(rec + sizeof(id), &dim, sizeof(dim));
        for (int i = 0; i < DIM; ++i) {
            float v = i * 0.5f;
            memcpy(rec + sizeof(id) + sizeof(dim) + i * sizeof(float), &v, sizeof(v));
        }
    }
    assert(ops->tuple_insert(rel, rec, sizeof(rec)) == 0);
    assert(ops->table_close(rel) == 0);

    assert(mm_storage_shutdown() == 0);
    printf("mm_vector_smoke OK\n");
    return 0;
}
