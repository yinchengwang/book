/**
 * @file test_trigger_functions_simple.cpp
 * @brief SQL 触发器函数注册与调用机制单元测试（简化版）
 *
 * 不依赖完整的 executor.h，直接测试触发器函数注册表。
 *
 * 本文件是 SQL 执行引擎 Task 5.5 触发器调用机制测试。
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "db/sql/trigger_functions.h"
#include "db/sql/trigger.h"
}

/* ========================================================================
 * 简化 TupleTableSlot 定义（用于测试）
 *
 * 由于 trigger.h 中 TupleTableSlot 是前向声明，
 * 我们使用 void* 指针来存储测试数据。
 * ======================================================================== */

/* 测试数据上下文 */
typedef struct {
    void *values;     /* 模拟 tts_values */
    void *isnull;     /* 模拟 tts_isnull */
    int nvalid;       /* 模拟 tts_nvalid */
} TestSlotContext;

/**
 * @brief 创建测试用 TriggerData
 *
 * 注意：由于 TupleTableSlot 在 trigger.h 中是前向声明，
 * 我们无法直接访问其内部字段。这里使用 tg_trigslot->type
 * 作为唯一可访问的字段来验证结构体存在。
 */
static TriggerData *
create_test_trigger_data(Oid func_oid, TriggerEvent event)
{
    TriggerDesc *desc = CreateTriggerDesc(
        "test_trigger",
        TRIGGER_TYPE_BEFORE_INSERT,
        100,
        func_oid,
        0, nullptr, true
    );
    if (desc == NULL)
        return NULL;

    TriggerData *data = (TriggerData *)calloc(1, sizeof(TriggerData));
    if (data == NULL) {
        DestroyTriggerDesc(desc);
        return NULL;
    }

    data->type = T_TriggerData;
    data->tg_desc = desc;
    data->tg_event = event;

    /* 创建简化的元组槽（仅分配基本内存） */
    /* 由于 TupleTableSlot 是前向声明，我们只能分配其大小 */
    /* 但不能安全地访问其内部字段 */
    data->tg_trigslot = (TupleTableSlot *)calloc(1, sizeof(TupleTableSlot));
    if (data->tg_trigslot == NULL) {
        free(data);
        DestroyTriggerDesc(desc);
        return NULL;
    }
    data->tg_trigslot->type = T_TupleTableSlot;

    return data;
}

/**
 * @brief 销毁测试 TriggerData
 */
static void
destroy_test_trigger_data(TriggerData *data)
{
    if (data == NULL)
        return;

    if (data->tg_trigslot != NULL)
        free(data->tg_trigslot);

    if (data->tg_desc != NULL)
        DestroyTriggerDesc(data->tg_desc);

    free(data);
}

/* ========================================================================
 * 触发器函数注册表测试
 * ======================================================================== */

class TriggerFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化注册表（如果尚未初始化） */
        if (GetTriggerFunctionCount() == 0) {
            InitTriggerFunctionRegistry();
        }
    }

    void TearDown() override {
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
 * @brief 测试重复注册（应失败）
 */
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
 * @brief 测试 CallTriggerFunction 封装函数
 */
TEST_F(TriggerTestIntegration, CallTriggerFunctionWrapper)
{
    TriggerData *data = create_test_trigger_data(
        TRIGGER_FUNC_AUTO_TIMESTAMP,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data, nullptr);

    /* 使用封装函数调用 */
    TupleTableSlot *result = CallTriggerFunction(data);
    EXPECT_NE(result, nullptr);

    destroy_test_trigger_data(data);
}

/**
 * @brief 测试未注册函数的调用
 */
TEST_F(TriggerTestIntegration, CallUnregisteredFunction)
{
    TriggerData *data = create_test_trigger_data(
        (Oid)9999,  /* 未注册的 OID */
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data, nullptr);

    /* 调用未注册的函数，应返回原槽 */
    TupleTableSlot *result = CallTriggerFunction(data);
    EXPECT_EQ(result, data->tg_trigslot);  /* 未注册时返回原槽 */

    destroy_test_trigger_data(data);
}

/**
 * @brief 测试 NULL 参数处理
 */
TEST_F(TriggerTestIntegration, NullParameterHandling)
{
    /* NULL 参数应返回 NULL */
    EXPECT_EQ(CallTriggerFunction(nullptr), nullptr);
}

/**
 * @brief 测试触发器链执行
 */
TEST_F(TriggerTestIntegration, TriggerChainExecution)
{
    /* 创建两个触发器 */
    TriggerData *data1 = create_test_trigger_data(
        TRIGGER_FUNC_AUTO_TIMESTAMP,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data1, nullptr);

    TriggerData *data2 = create_test_trigger_data(
        TRIGGER_FUNC_VALIDATE_DATA,
        TRIGGER_EVENT_BEFORE_INSERT
    );
    ASSERT_NE(data2, nullptr);

    /* 执行第一个触发器 */
    TriggerFunc func1 = FindTriggerFunction(data1->tg_desc->tgfuncid);
    ASSERT_NE(func1, nullptr);
    TupleTableSlot *result1 = func1(data1);
    EXPECT_NE(result1, nullptr);

    /* 执行第二个触发器 */
    TriggerFunc func2 = FindTriggerFunction(data2->tg_desc->tgfuncid);
    ASSERT_NE(func2, nullptr);
    TupleTableSlot *result2 = func2(data2);
    EXPECT_NE(result2, nullptr);

    /* 清理 */
    destroy_test_trigger_data(data1);
    destroy_test_trigger_data(data2);
}

/**
 * @brief 测试不同事件类型的触发器
 */
TEST_F(TriggerTestIntegration, DifferentEventTypes)
{
    /* 测试 BEFORE INSERT */
    {
        TriggerData *data = create_test_trigger_data(
            TRIGGER_FUNC_AUTO_TIMESTAMP,
            TRIGGER_EVENT_BEFORE_INSERT
        );
        ASSERT_NE(data, nullptr);
        TupleTableSlot *result = CallTriggerFunction(data);
        EXPECT_NE(result, nullptr);
        destroy_test_trigger_data(data);
    }

    /* 测试 AFTER INSERT */
    {
        TriggerData *data = create_test_trigger_data(
            TRIGGER_FUNC_AUDIT_LOG,
            TRIGGER_EVENT_AFTER_INSERT
        );
        ASSERT_NE(data, nullptr);
        TupleTableSlot *result = CallTriggerFunction(data);
        EXPECT_EQ(result, nullptr);  /* AFTER 触发器返回 NULL */
        destroy_test_trigger_data(data);
    }

    /* 测试 BEFORE UPDATE */
    {
        TriggerData *data = create_test_trigger_data(
            TRIGGER_FUNC_VALIDATE_DATA,
            TRIGGER_EVENT_BEFORE_UPDATE
        );
        ASSERT_NE(data, nullptr);
        TupleTableSlot *result = CallTriggerFunction(data);
        EXPECT_NE(result, nullptr);
        destroy_test_trigger_data(data);
    }

    /* 测试 AFTER DELETE */
    {
        TriggerData *data = create_test_trigger_data(
            TRIGGER_FUNC_AUDIT_LOG,
            TRIGGER_EVENT_AFTER_DELETE
        );
        ASSERT_NE(data, nullptr);
        TupleTableSlot *result = CallTriggerFunction(data);
        EXPECT_EQ(result, nullptr);  /* AFTER 触发器返回 NULL */
        destroy_test_trigger_data(data);
    }
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
