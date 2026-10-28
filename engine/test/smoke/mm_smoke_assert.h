/**
 * @file mm_smoke_assert.h
 * @brief 模态冒烟测试的共享断言（profile 感知）
 *
 * 断言「注册表中的非空槽位集合 == 本 profile 的期望引擎掩码」。
 *
 * 为什么不用硬编码计数：冒烟测试原先写死 n==1 / n==2，这在单模态
 * profile 下成立，但 legacy/full profile 会注册多个引擎，冒烟必然失败。
 * 期望掩码由构建期按 profile 计算（cmake/mm_smoke_expect.h.in），
 * 单模态下掩码只有本模态一位，语义与原断言完全等价。
 *
 * 另外还断言「本模态引擎已注册」——这是冒烟的核心命题，
 * 掩码相等只是排除多余引擎。
 */
#ifndef DB_MM_SMOKE_ASSERT_H
#define DB_MM_SMOKE_ASSERT_H

#include "db/mm_smoke_expect.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

/**
 * @brief 校验注册表槽位集合等于本 profile 期望掩码
 * @param own_model 本模态对应的 DataModel 槽位（RELATIONAL 模态传 MODEL_RELATIONAL）
 * @param expect_own 该槽位在本 profile 下是否应被注册
 * @return 实际注册引擎数
 */
static inline int mm_smoke_assert_engine_set(DataModel own_model, int expect_own) {
    unsigned int mask = 0u;
    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            mask |= (1u << (unsigned int)m);
            ++n;
        }
    }

    unsigned int expect = (unsigned int)MM_SMOKE_EXPECTED_ENGINE_MASK;
    if (mask != expect) {
        fprintf(stderr,
                "smoke: engine slot set mismatch\n"
                "  expected mask = 0x%x\n"
                "  actual   mask = 0x%x (%d slots)\n",
                expect, mask, n);
    }
    assert(mask == expect);

    if (expect_own) {
        assert(storage_get_engine(own_model) != NULL);
    }
    return n;
}

#endif /* DB_MM_SMOKE_ASSERT_H */
