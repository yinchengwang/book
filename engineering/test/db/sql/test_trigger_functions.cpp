/**
 * @file test_trigger_functions.cpp
 * @brief SQL 触发器函数注册与调用机制单元测试
 *
 * 测试触发器函数注册表、内置函数查找、自定义函数注册、
 * BEFORE/AFTER 触发器执行等功能。
 *
 * 本文件是 SQL 执行引擎 Task 5.5 触发器调用机制测试。
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/trigger_functions.h"
#include "db/sql/trigger.h"
#include "db/sql/executor.h"
}

/* ========================================================================
 * 触发器函数注册表测试
 * ======================================================================== */

class TriggerFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 每个测试前的初始化 */
        /* 初始化注册表（如果尚未初始化） */
        if (GetTriggerFunctionCount() == 0) {
            InitTriggerFunctionRegistry();
        }
    }

    void TearDown() override {
        /* 每个测试后的清理 */
        /* 不销毁注册表，保持内置函数可用 */
    }
};

/**
 * @brief 测试初始化注册表
 */
TEST_F(TriggerFunctionsTest, InitRegistry)
{
    /* 销毁现有注册表以测试重新初始化 */
    DestroyTriggerFunctionRegistry();

    /* 验证注册表为空 */
    EXPECT_EQ(GetTriggerFunctionCount(), 0);

    /* 初始化注册表 */
    int result = InitTriggerFunctionRegistry();
    EXPECT_EQ(result, 0);

    /* 验证注册了 3 个内置函数 */
    EXPECT_EQ(GetTriggerFunctionCount(), 3);

    /* 验证内置函数存在 */
    EXPECT_TRUE(TriggerFunctionExists(TRIGGER_FUNC_AUTO_TIMESTAMP));
    EXPECT_TRUE(TriggerFunctionExists(TRIGGER_FUNC_AUDIT_LOG));
    EXPECT_TRUE(TriggerFunctionExists(TRIGGER_FUNC_VALIDATE_DATA));
}

/**
 * @brief 测试查找内置函数
 */
TEST_F(TriggerFunctionsTest, FindBuiltin)
{
    /* 查找 auto_timestamp */
    TriggerFunc func1 = FindTriggerFunction(TRIGGER_FUNC_AUTO_TIMESTAMP);
    EXPECT_NE(func1, nullptr);
    EXPECT_EQ(func1, trigger_auto_timestamp);

    /* 查找 audit_log */
    TriggerFunc func2 = FindTriggerFunction(TRIGGER_FUNC_AUDIT_LOG);
    EXPECT_NE(func2, nullptr);
    EXPECT_EQ(func2, trigger_audit_log);

    /* 查找 validate_data */
    TriggerFunc func3 = FindTriggerFunction(TRIGGER_FUNC_VALIDATE_DATA);
    EXPECT_NE(func3, nullptr);
    EXPECT_EQ(func3, trigger_validate_data);
}

/**
 * @brief 测试按名称查找
 */
TEST_F(TriggerFunctionsTest, FindByName)
{
    /* 查找 auto_timestamp */
    TriggerFunc func1 = FindTriggerFunctionByName("auto_timestamp");
    EXPECT_NE(func1, nullptr);
    EXPECT_EQ(func1, trigger_auto_timestamp);

    /* 查找 audit_log */
    TriggerFunc func2 = FindTriggerFunctionByName("audit_log");
    EXPECT_NE(func2, nullptr);
    EXPECT_EQ(func2, trigger_audit_log);

    /* 查找 validate_data */
    TriggerFunc func3 = FindTriggerFunctionByName("validate_data");
    EXPECT_NE(func3, nullptr);
    EXPECT_EQ(func3, trigger_validate_data);
}

/**
 * @brief 测试查找不存在的函数
 */
TEST_F(TriggerFunctionsTest, FindNonExistent)
{
    /* 查找不存在的 OID */
    TriggerFunc func = FindTriggerFunction((Oid)9999);
    EXPECT_EQ(func, nullptr);

    /* 按名称查找不存在的函数 */
    func = FindTriggerFunctionByName("nonexistent_function");
    EXPECT_EQ(func, nullptr);

    /* 检查不存在的函数 */
    EXPECT_FALSE(TriggerFunctionExists((Oid)9999));
}

/**
 * @brief 测试注册自定义函数
 */
TEST_F(TriggerFunctionsTest, RegisterCustom)
{
    /* 注册一个自定义触发器函数 */
    auto custom_func = [](TriggerData *trig_data) -> TupleTableSlot * {
        return trig_data->tg_trigslot;
    };

    int result = RegisterTriggerFunction("custom_trigger",
                                         (Oid)2000,
                                         custom_func,
                                         false);
    EXPECT_EQ(result, 0);

    /* 验证注册成功 */
    EXPECT_TRUE(TriggerFunctionExists((Oid)2000));
    EXPECT_EQ(GetTriggerFunctionCount(), 4);  /* 3 内置 + 1 自定义 */

    /* 验证可以查找 */
    TriggerFunc func = FindTriggerFunction((Oid)2000);
    EXPECT_NE(func, nullptr);
    EXPECT_EQ(func, custom_func);

    /* 按名称查找 */
    func = FindTriggerFunctionByName("custom_trigger");
    EXPECT_NE(func, nullptr);
    EXPECT_EQ(func, custom_func);
}

/**
 * @brief 测试重复注册（应失败） */
TEST_F(TriggerFunctionsTest, RegisterDuplicate)
{
    /* 尝试注册相同 OID 的函数（应失败） */
    int result = RegisterTriggerFunction("duplicate_trigger",
                                         TRIGGER_FUNC_AUTO_TIMESTAMP,
                                         trigger_audit_log,
                                         false);
    EXPECT_NE(result, 0);  /* 应返回错误 */

    /* 验证函数未被替换 */
    TriggerFunc func = FindTriggerFunction(TRIGGER_FUNC_AUTO_TIMESTAMP);
    EXPECT_EQ(func, trigger_auto_timestamp);
}

/**
 * @brief 测试获取函数名称
 */
TEST_F(TriggerFunctionsTest, GetFunctionName)
{
    const char *name = GetTriggerFunctionName(TRIGGER_FUNC_AUTO_TIMESTAMP);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "auto_timestamp");

    name = GetTriggerFunctionName(TRIGGER_FUNC_AUDIT_LOG);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "audit_log");

    name = GetTriggerFunctionName(TRIGGER_FUNC_VALIDATE_DATA);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "validate_data");

    /* 不存在的函数 */
    name = GetTriggerFunctionName((Oid)9999);
    EXPECT_EQ(name, nullptr);
}

/* ========================================================================
 * 内置触发器函数执行测试
 * ======================================================================== */

/**
 * @brief 测试执行内置函数
 */
TEST_F(TriggerFunctionsTest, ExecBuiltinFunction)
{
    /* 创建测试上下文 */
    TriggerDesc *desc = CreateTriggerDesc(
        "test_trigger",
        TRIGGER_TYPE_BEFORE_INSERT,
        100,
        TRIGGER_FUNC_AUTO_TIMESTAMP,
        0, nullptr, true
    );
    ASSERT_NE(desc, nullptr);

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 设置槽的值 */
    if (slot->tts_values != nullptr) {
        slot->tts_values[0] = Int32GetDatum(42);
        slot->tts_isnull[0] = false;
        slot->tts_nvalid = 1;
    }

    /* 创建 TriggerData */
    TriggerData *data = CreateTriggerData(
        desc, estate, nullptr, slot, nullptr,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data, nullptr);

    /* 执行触发器函数 */
    TriggerFunc func = FindTriggerFunction(desc->tgfuncid);
    ASSERT_NE(func, nullptr);

    TupleTableSlot *result = func(data);
    EXPECT_NE(result, nullptr);

    /* 清理 */
    DestroyTriggerData(data);
    FreeTupleTableSlot(slot);
    FreeEState(estate);
    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试审计日志函数
 */
TEST_F(TriggerFunctionsTest, ExecAuditLog)
{
    /* 创建测试上下文 */
    TriggerDesc *desc = CreateTriggerDesc(
        "audit_trigger",
        TRIGGER_TYPE_AFTER_INSERT,
        100,
        TRIGGER_FUNC_AUDIT_LOG,
        0, nullptr, true
    );
    ASSERT_NE(desc, nullptr);

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 创建 TriggerData */
    TriggerData *data = CreateTriggerData(
        desc, estate, nullptr, slot, nullptr,
        TRIGGER_EVENT_AFTER_INSERT
    );
    ASSERT_NE(data, nullptr);

    /* 执行审计日志函数（AFTER 触发器返回 NULL） */
    TriggerFunc func = FindTriggerFunction(TRIGGER_FUNC_AUDIT_LOG);
    ASSERT_NE(func, nullptr);

    TupleTableSlot *result = func(data);
    EXPECT_EQ(result, nullptr);  /* AFTER 触发器不修改元组 */

    /* 清理 */
    DestroyTriggerData(data);
    FreeTupleTableSlot(slot);
    FreeEState(estate);
    DestroyTriggerDesc(desc);
}

/* ========================================================================
 * 触发器执行集成测试
 * ======================================================================== */

class TriggerTestIntegration : public ::testing::Test {
protected:
    void SetUp() override {
        /* 确保注册表已初始化 */
        if (GetTriggerFunctionCount() == 0) {
            InitTriggerFunctionRegistry();
        }
    }

    void TearDown() override {
        /* 不销毁注册表 */
    }
};

/**
 * @brief 测试 BEFORE INSERT 触发器修改元组
 */
TEST_F(TriggerTestIntegration, BeforeInsertModifyTuple)
{
    /* 创建测试上下文 */
    TriggerDesc *desc = CreateTriggerDesc(
        "before_insert_trigger",
        TRIGGER_TYPE_BEFORE_INSERT,
        100,
        TRIGGER_FUNC_AUTO_TIMESTAMP,
        0, nullptr, true
    );
    ASSERT_NE(desc, nullptr);

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 初始状态：第一个字段为空 */
    if (slot->tts_values != nullptr && slot->tts_isnull != nullptr) {
        slot->tts_values[0] = 0;
        slot->tts_isnull[0] = true;  /* 初始为空 */
        slot->tts_nvalid = 1;
    }

    /* 创建 TriggerData */
    TriggerData *data = CreateTriggerData(
        desc, estate, nullptr, slot, nullptr,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data, nullptr);

    /* 执行触发器函数 */
    TriggerFunc func = FindTriggerFunction(desc->tgfuncid);
    ASSERT_NE(func, nullptr);

    TupleTableSlot *result = func(data);
    EXPECT_NE(result, nullptr);

    /* 验证元组被修改（字段不再为空） */
    if (slot->tts_isnull != nullptr) {
        EXPECT_FALSE(slot->tts_isnull[0]);
    }

    /* 清理 */
    DestroyTriggerData(data);
    FreeTupleTableSlot(slot);
    FreeEState(estate);
    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试 AFTER 触发器审计
 */
TEST_F(TriggerTestIntegration, AfterTriggerAudit)
{
    /* 创建测试上下文 */
    TriggerDesc *desc = CreateTriggerDesc(
        "after_insert_audit",
        TRIGGER_TYPE_AFTER_INSERT,
        100,
        TRIGGER_FUNC_AUDIT_LOG,
        0, nullptr, true
    );
    ASSERT_NE(desc, nullptr);

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 创建 TriggerData */
    TriggerData *data = CreateTriggerData(
        desc, estate, nullptr, slot, nullptr,
        TRIGGER_EVENT_AFTER_INSERT
    );
    ASSERT_NE(data, nullptr);

    /* 执行触发器函数 */
    TriggerFunc func = FindTriggerFunction(desc->tgfuncid);
    ASSERT_NE(func, nullptr);

    TupleTableSlot *result = func(data);
    EXPECT_EQ(result, nullptr);  /* AFTER 触发器返回 NULL */

    /* 清理 */
    DestroyTriggerData(data);
    FreeTupleTableSlot(slot);
    FreeEState(estate);
    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试触发器链执行
 */
TEST_F(TriggerTestIntegration, TriggerChainExecution)
{
    /* 创建两个触发器 */
    TriggerDesc *desc1 = CreateTriggerDesc(
        "chain_trigger_1",
        TRIGGER_TYPE_BEFORE_INSERT,
        100,
        TRIGGER_FUNC_AUTO_TIMESTAMP,
        0, nullptr, true
    );
    ASSERT_NE(desc1, nullptr);

    TriggerDesc *desc2 = CreateTriggerDesc(
        "chain_trigger_2",
        TRIGGER_TYPE_BEFORE_INSERT,
        100,
        TRIGGER_FUNC_VALIDATE_DATA,
        0, nullptr, true
    );
    ASSERT_NE(desc2, nullptr);

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 设置槽的值 */
    if (slot->tts_values != nullptr && slot->tts_isnull != nullptr) {
        slot->tts_values[0] = Int32GetDatum(100);
        slot->tts_isnull[0] = false;
        slot->tts_nvalid = 1;
    }

    /* 执行第一个触发器 */
    TriggerData *data1 = CreateTriggerData(
        desc1, estate, nullptr, slot, nullptr,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data1, nullptr);

    TriggerFunc func1 = FindTriggerFunction(desc1->tgfuncid);
    ASSERT_NE(func1, nullptr);

    TupleTableSlot *result1 = func1(data1);
    EXPECT_NE(result1, nullptr);

    /* 执行第二个触发器 */
    TriggerData *data2 = CreateTriggerData(
        desc2, estate, nullptr, slot, nullptr,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data2, nullptr);

    TriggerFunc func2 = FindTriggerFunction(desc2->tgfuncid);
    ASSERT_NE(func2, nullptr);

    TupleTableSlot *result2 = func2(data2);
    EXPECT_NE(result2, nullptr);

    /* 清理 */
    DestroyTriggerData(data1);
    DestroyTriggerData(data2);
    FreeTupleTableSlot(slot);
    FreeEState(estate);
    DestroyTriggerDesc(desc1);
    DestroyTriggerDesc(desc2);
}

/**
 * @brief 测试 CallTriggerFunction 封装函数
 */
TEST_F(TriggerTestIntegration, CallTriggerFunctionWrapper)
{
    /* 创建测试上下文 */
    TriggerDesc *desc = CreateTriggerDesc(
        "wrapper_test",
        TRIGGER_TYPE_BEFORE_INSERT,
        100,
        TRIGGER_FUNC_AUTO_TIMESTAMP,
        0, nullptr, true
    );
    ASSERT_NE(desc, nullptr);

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 创建 TriggerData */
    TriggerData *data = CreateTriggerData(
        desc, estate, nullptr, slot, nullptr,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data, nullptr);

    /* 使用封装函数调用 */
    TupleTableSlot *result = CallTriggerFunction(data);
    EXPECT_NE(result, nullptr);

    /* 清理 */
    DestroyTriggerData(data);
    FreeTupleTableSlot(slot);
    FreeEState(estate);
    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试未注册函数的调用
 */
TEST_F(TriggerTestIntegration, CallUnregisteredFunction)
{
    /* 创建指向未注册函数的触发器 */
    TriggerDesc *desc = CreateTriggerDesc(
        "unregistered_trigger",
        TRIGGER_TYPE_BEFORE_INSERT,
        100,
        (Oid)9999,  /* 未注册的 OID */
        0, nullptr, true
    );
    ASSERT_NE(desc, nullptr);

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 创建 TriggerData */
    TriggerData *data = CreateTriggerData(
        desc, estate, nullptr, slot, nullptr,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data, nullptr);

    /* 调用未注册的函数，应返回原槽 */
    TupleTableSlot *result = CallTriggerFunction(data);
    EXPECT_EQ(result, slot);  /* 未注册时返回原槽 */

    /* 清理 */
    DestroyTriggerData(data);
    FreeTupleTableSlot(slot);
    FreeEState(estate);
    DestroyTriggerDesc(desc);
}

/**
 * @brief 测试 NULL 参数处理
 */
TEST_F(TriggerTestIntegration, NullParameterHandling)
{
    /* NULL 参数应返回 NULL */
    EXPECT_EQ(CallTriggerFunction(nullptr), nullptr);
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
