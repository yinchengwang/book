/**
 * @file mm_vector_smoke.c
 * @brief 向量模态独立冒烟测试
 *
 * 链接期证明：本目标只链 mm_base + mm_storage_api + mm_vector
 * （vector_index 子库经 mm_vector PUBLIC 传递），不链 db_storage/db_core。
 * 运行期证明：只注册 VECTOR 一个引擎，且能经 storage_ops_t 完成
 * table_create -> table_open -> tuple_insert 真实写入。
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_vector") == 0);

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            ++n;
        }
    }
    /* 独立性证明：vector profile 下只应有一个引擎 */
    assert(n == 1);
    const storage_ops_t *ops = storage_get_engine(MODEL_VECTOR);
    assert(ops != NULL);
    assert(storage_get_engine(MODEL_KV) == NULL);
    assert(storage_get_engine(MODEL_GRAPH) == NULL);

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
