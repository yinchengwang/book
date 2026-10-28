/**
 * @file mm_graph_smoke.c
 * @brief 图模态独立冒烟测试
 *
 * 链接期证明：只链 mm_base + mm_storage_api + mm_graph（kv 经
 * mm_graph PUBLIC 传递，graph 白名单依赖 kv）。
 * 运行期证明：graph profile 下注册 GRAPH 与 KV 共 2 个引擎。
 */
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_graph") == 0);

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            ++n;
        }
    }
    /* graph profile 闭包 = graph + kv（graph 白名单依赖 kv） */
    assert(n == 2);
    assert(storage_get_engine(MODEL_GRAPH) != NULL);
    assert(storage_get_engine(MODEL_KV) != NULL);
    assert(storage_get_engine(MODEL_VECTOR) == NULL);

    assert(mm_storage_shutdown() == 0);
    printf("mm_graph_smoke OK\n");
    return 0;
}
