/**
 * @file test_trigger.cpp
 * @brief SQL 触发器系统单元测试
 *
 * 测试触发器描述符管理、触发器执行逻辑、事件匹配等功能。
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/trigger.h"
#include "db/sql/executor.h"
#include "db/sql/nodes/execnodes.h"
}

/* ========================================================================
 * 触发器描述符测试
 * ======================================================================== */

class TriggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 每个测试前的初始化 */
    }

    void TearDown() override {
        /* 每个测试后的清理 */
    }
};

/**
 * @brief 测试触发器描述符创建
 */
TEST_F(TriggerTest, DescriptorCreate)
{
    const char *name = "test_trigger";
    Datum args[2] = { Int32GetDatum(1), Int32GetDatum(2) };

    TriggerDesc *desc = CreateTriggerDesc(
        name,
        TRIGGER_TYPE_BEFORE_INSERT,
        100,  /* tgrelid */
        200,  /* tgfuncid */
        2,    /* tgnargs */
        args,
        true  /* tgenabled */
    );

    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->type, T_TriggerDesc);
    EXPECT_NE(desc->tgname, nullptr);
    EXPECT_STREQ(desc->tgname, name);
    EXPECT_EQ(desc->tgtype, TRIGGER_TYPE_BEFORE_INSERT);
    EXPECT_EQ(desc->tgrelid, (Oid)100);
    EXPECT_EQ(desc->tgfuncid, (Oid)200);
    EXPECT_EQ(desc->tgnargs, 2);
    EXPECT_NE(desc->tgargs, nullptr);
    EXPECT_EQ(desc->tgenabled, true);
    EXPECT_EQ(desc->tgdeferrable, false);
    EXPECT_EQ(desc->tginitdeferred, false);

    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试销毁空触发器描述符（不应崩溃）
 */
TEST_F(TriggerTest, DescriptorNullDestroy)
{
    /* 销毁 NULL 应该安全无操作 */
    DestroyTriggerDesc(nullptr);
    EXPECT_TRUE(true);  /* 到达此处表示无崩溃 */
}

/**
 * @brief 测试无参数触发器描述符创建
 */
TEST_F(TriggerTest, DescriptorNoArgs)
{
    const char *name = "no_args_trigger";

    TriggerDesc *desc = CreateTriggerDesc(
        name,
        TRIGGER_TYPE_AFTER_UPDATE,
        100,  /* tgrelid */
        200,  /* tgfuncid */
        0,    /* tgnargs */
        nullptr,
        true  /* tgenabled */
    );

    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->tgnargs, 0);
    EXPECT_EQ(desc->tgargs, nullptr);

    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试禁用的触发器描述符
 */
TEST_F(TriggerTest, DescriptorDisabled)
{
    const char *name = "disabled_trigger";

    TriggerDesc *desc = CreateTriggerDesc(
        name,
        TRIGGER_TYPE_BEFORE_DELETE,
        100,
        200,
        0,
        nullptr,
        false  /* tgenabled */
    );

    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->tgenabled, false);

    /* 禁用的触发器不应匹配任何事件 */
    EXPECT_FALSE(TriggerMatchesEvent(desc, TRIGGER_EVENT_BEFORE_DELETE));

    DestroyTriggerDesc(desc);
}

/* ========================================================================
 * 事件匹配测试
 * ======================================================================== */

/**
 * @brief 测试 BEFORE INSERT 事件匹配
 */
TEST_F(TriggerTest, TriggerMatchesBeforeInsert)
{
    TriggerDesc *desc = CreateTriggerDesc(
        "bi_trigger",
        TRIGGER_TYPE_BEFORE_INSERT,
        100, 200, 0, nullptr, true
    );

    ASSERT_NE(desc, nullptr);

    EXPECT_TRUE(TriggerMatchesEvent(desc, TRIGGER_EVENT_BEFORE_INSERT));
    EXPECT_FALSE(TriggerMatchesEvent(desc, TRIGGER_EVENT_AFTER_INSERT));
    EXPECT_FALSE(TriggerMatchesEvent(desc, TRIGGER_EVENT_BEFORE_UPDATE));
    EXPECT_FALSE(TriggerMatchesEvent(desc, TRIGGER_EVENT_BEFORE_DELETE));

    EXPECT_TRUE(TriggerIsBefore(desc));
    EXPECT_FALSE(TriggerIsAfter(desc));

    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试 AFTER UPDATE 事件匹配
 */
TEST_F(TriggerTest, TriggerMatchesAfterUpdate)
{
    TriggerDesc *desc = CreateTriggerDesc(
        "au_trigger",
        TRIGGER_TYPE_AFTER_UPDATE,
        100, 200, 0, nullptr, true
    );

    ASSERT_NE(desc, nullptr);

    EXPECT_TRUE(TriggerMatchesEvent(desc, TRIGGER_EVENT_AFTER_UPDATE));
    EXPECT_FALSE(TriggerMatchesEvent(desc, TRIGGER_EVENT_BEFORE_UPDATE));
    EXPECT_FALSE(TriggerMatchesEvent(desc, TRIGGER_EVENT_AFTER_INSERT));
    EXPECT_FALSE(TriggerMatchesEvent(desc, TRIGGER_EVENT_AFTER_DELETE));

    EXPECT_FALSE(TriggerIsBefore(desc));
    EXPECT_TRUE(TriggerIsAfter(desc));

    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试多事件触发器
 */
TEST_F(TriggerTest, TriggerMatchesMultipleEvents)
{
    /* 触发器响应 INSERT 和 UPDATE 事件 */
    TriggerType type = (TriggerType)(TRIGGER_TYPE_BEFORE_INSERT | TRIGGER_TYPE_BEFORE_UPDATE);

    TriggerDesc *desc = CreateTriggerDesc(
        "multi_trigger",
        type,
        100, 200, 0, nullptr, true
    );

    ASSERT_NE(desc, nullptr);

    EXPECT_TRUE(TriggerMatchesEvent(desc, TRIGGER_EVENT_BEFORE_INSERT));
    EXPECT_TRUE(TriggerMatchesEvent(desc, TRIGGER_EVENT_BEFORE_UPDATE));
    EXPECT_FALSE(TriggerMatchesEvent(desc, TRIGGER_EVENT_BEFORE_DELETE));
    EXPECT_FALSE(TriggerMatchesEvent(desc, TRIGGER_EVENT_AFTER_INSERT));

    DestroyTriggerDesc(desc);
}

/* ========================================================================
 * TriggerData 测试
 * ======================================================================== */

/**
 * @brief 测试 TriggerData 创建与销毁
 */
TEST_F(TriggerTest, TriggerDataCreateDestroy)
{
    TriggerDesc *desc = CreateTriggerDesc(
        "test_trigger",
        TRIGGER_TYPE_BEFORE_INSERT,
        100, 200, 0, nullptr, true
    );
    ASSERT_NE(desc, nullptr);

    /* 创建 EState 用于测试 */
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    /* 创建元组槽 */
    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 创建 TriggerData */
    TriggerData *data = CreateTriggerData(
        desc,
        estate,
        nullptr,  /* rrinfo */
        slot,
        nullptr,  /* newslot */
        TRIGGER_EVENT_BEFORE_INSERT
    );

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->type, T_TriggerData);
    EXPECT_EQ(data->tg_desc, desc);
    EXPECT_EQ(data->tg_estate, estate);
    EXPECT_EQ(data->tg_trigslot, slot);
    EXPECT_EQ(data->tg_newslot, nullptr);
    EXPECT_EQ(data->tg_event, TRIGGER_EVENT_BEFORE_INSERT);

    DestroyTriggerData(data);
    FreeTupleTableSlot(slot);
    FreeEState(estate);
    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试销毁空 TriggerData（不应崩溃）
 */
TEST_F(TriggerTest, TriggerDataNullDestroy)
{
    DestroyTriggerData(nullptr);
    EXPECT_TRUE(true);
}

/* ========================================================================
 * 触发器执行测试
 * ======================================================================== */

/**
 * @brief 测试 ExecTrigger 简化实现
 */
TEST_F(TriggerTest, ExecTrigger)
{
    TriggerDesc *desc = CreateTriggerDesc(
        "exec_trigger",
        TRIGGER_TYPE_BEFORE_INSERT,
        100, 200, 0, nullptr, true
    );
    ASSERT_NE(desc, nullptr);

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    TriggerData *data = CreateTriggerData(
        desc, estate, nullptr, slot, nullptr,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data, nullptr);

    /* 执行触发器（简化实现：返回传入元组） */
    TupleTableSlot *result = ExecTrigger(data);
    EXPECT_EQ(result, slot);

    DestroyTriggerData(data);
    FreeTupleTableSlot(slot);
    FreeEState(estate);
    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试 BEFORE 触发器执行
 */
TEST_F(TriggerTest, ExecBeforeTrigger)
{
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 执行 BEFORE 触发器链（简化实现：返回传入元组） */
    TupleTableSlot *result = ExecBeforeTriggers(
        estate,
        nullptr,  /* rrinfo */
        TRIGGER_EVENT_BEFORE_INSERT,
        slot,
        nullptr
    );

    EXPECT_EQ(result, slot);

    FreeTupleTableSlot(slot);
    FreeEState(estate);
}

/**
 * @brief 测试 AFTER 触发器执行
 */
TEST_F(TriggerTest, ExecAfterTrigger)
{
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 执行 AFTER 触发器链（简化实现：无操作） */
    ExecAfterTriggers(
        estate,
        nullptr,  /* rrinfo */
        TRIGGER_EVENT_AFTER_INSERT,
        slot
    );

    /* 无崩溃即成功 */

    FreeTupleTableSlot(slot);
    FreeEState(estate);
}

/**
 * @brief 测试触发器链（多个触发器）
 */
TEST_F(TriggerTest, TriggerChain)
{
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot1 = MakeTupleTableSlot();
    TupleTableSlot *slot2 = MakeTupleTableSlot();
    ASSERT_NE(slot1, nullptr);
    ASSERT_NE(slot2, nullptr);

    /* 创建两个触发器描述符 */
    TriggerDesc *desc1 = CreateTriggerDesc(
        "chain_trigger_1",
        TRIGGER_TYPE_BEFORE_INSERT,
        100, 200, 0, nullptr, true
    );
    TriggerDesc *desc2 = CreateTriggerDesc(
        "chain_trigger_2",
        TRIGGER_TYPE_BEFORE_INSERT,
        100, 201, 0, nullptr, true
    );
    ASSERT_NE(desc1, nullptr);
    ASSERT_NE(desc2, nullptr);

    /* 创建 TriggerData 并执行 */
    TriggerData *data1 = CreateTriggerData(
        desc1, estate, nullptr, slot1, nullptr,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data1, nullptr);

    TupleTableSlot *result = ExecTrigger(data1);
    EXPECT_EQ(result, slot1);

    /* 第二个触发器 */
    TriggerData *data2 = CreateTriggerData(
        desc2, estate, nullptr, slot2, nullptr,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data2, nullptr);

    result = ExecTrigger(data2);
    EXPECT_EQ(result, slot2);

    /* 清理 */
    DestroyTriggerData(data1);
    DestroyTriggerData(data2);
    FreeTupleTableSlot(slot1);
    FreeTupleTableSlot(slot2);
    FreeEState(estate);
    DestroyTriggerDesc(desc1);
    DestroyTriggerDesc(desc2);
}

/**
 * @brief 测试空触发器执行
 */
TEST_F(TriggerTest, NullTrigger)
{
    /* 执行 NULL 触发器应返回 NULL */
    TupleTableSlot *result = ExecTrigger(nullptr);
    EXPECT_EQ(result, nullptr);

    /* ExecBeforeTriggers 传入 NULL 参数 */
    result = ExecBeforeTriggers(nullptr, nullptr, TRIGGER_EVENT_BEFORE_INSERT, nullptr, nullptr);
    EXPECT_EQ(result, nullptr);

    /* ExecAfterTriggers 传入 NULL 参数（不应崩溃） */
    ExecAfterTriggers(nullptr, nullptr, TRIGGER_EVENT_AFTER_INSERT, nullptr);
    EXPECT_TRUE(true);
}

/* ========================================================================
 * 宏测试
 * ======================================================================== */

/**
 * @brief 测试触发器事件宏
 */
TEST_F(TriggerTest, TriggerEventMacros)
{
    /* BEFORE INSERT */
    EXPECT_TRUE(TRIGGER_FIRED_BEFORE_INSERT(TRIGGER_EVENT_BEFORE_INSERT));
    EXPECT_FALSE(TRIGGER_FIRED_AFTER_INSERT(TRIGGER_EVENT_BEFORE_INSERT));

    /* AFTER INSERT */
    EXPECT_TRUE(TRIGGER_FIRED_AFTER_INSERT(TRIGGER_EVENT_AFTER_INSERT));
    EXPECT_FALSE(TRIGGER_FIRED_BEFORE_INSERT(TRIGGER_EVENT_AFTER_INSERT));

    /* BEFORE UPDATE */
    EXPECT_TRUE(TRIGGER_FIRED_BEFORE_UPDATE(TRIGGER_EVENT_BEFORE_UPDATE));
    EXPECT_FALSE(TRIGGER_FIRED_BEFORE_DELETE(TRIGGER_EVENT_BEFORE_UPDATE));

    /* AFTER UPDATE */
    EXPECT_TRUE(TRIGGER_FIRED_AFTER_UPDATE(TRIGGER_EVENT_AFTER_UPDATE));

    /* BEFORE DELETE */
    EXPECT_TRUE(TRIGGER_FIRED_BEFORE_DELETE(TRIGGER_EVENT_BEFORE_DELETE));

    /* AFTER DELETE */
    EXPECT_TRUE(TRIGGER_FIRED_AFTER_DELETE(TRIGGER_EVENT_AFTER_DELETE));

    /* BEFORE/AFTER 检查 */
    EXPECT_TRUE(TRIGGER_FIRED_BEFORE(TRIGGER_EVENT_BEFORE_INSERT));
    EXPECT_FALSE(TRIGGER_FIRED_AFTER(TRIGGER_EVENT_BEFORE_INSERT));
    EXPECT_TRUE(TRIGGER_FIRED_AFTER(TRIGGER_EVENT_AFTER_INSERT));
    EXPECT_FALSE(TRIGGER_FIRED_BEFORE(TRIGGER_EVENT_AFTER_INSERT));

    /* 行级检查 */
    EXPECT_TRUE(TRIGGER_FIRED_FOR_ROW(TRIGGER_EVENT_BEFORE_INSERT));
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}