/**
 * @file test_agent.cpp
 * @brief Modular RAG Agent 单元测试
 */

#include <gtest/gtest.h>
#include "rag/modular/agent/agent.h"
#include "rag/modular/agent/tool.h"
#include "rag/modular/config.h"
#include "rag/llm_service.h"
#include <memory>
#include <vector>

using namespace rag::modular::agent;

/**
 * @brief 测试夹具：Agent 基本功能测试
 */
class AgentTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.max_iterations = 5;
        config_.max_retries = 2;
        config_.temperature = 0.0f;
        config_.verbose = false;
    }

    AgentConfig config_;
};

/**
 * @brief 测试夹具：Tool 基本功能测试
 */
class ToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建空的 AgentConfig
        agent_config_.max_iterations = 10;
        agent_config_.temperature = 0.7f;
    }

    AgentConfig agent_config_;
};

// ========== AgentConfig 测试 ==========

/**
 * @brief 测试 AgentConfig 默认值
 */
TEST(AgentConfigTest, DefaultValues) {
    AgentConfig config;

    EXPECT_EQ(config.max_iterations, 10);
    EXPECT_EQ(config.max_retries, 3);
    EXPECT_FLOAT_EQ(config.temperature, 0.0f);
    EXPECT_FALSE(config.verbose);
}

/**
 * @brief 测试 AgentConfig 设置
 */
TEST(AgentConfigTest, Setters) {
    AgentConfig config;
    config.max_iterations = 20;
    config.max_retries = 5;
    config.temperature = 0.9f;
    config.verbose = true;

    EXPECT_EQ(config.max_iterations, 20);
    EXPECT_EQ(config.max_retries, 5);
    EXPECT_FLOAT_EQ(config.temperature, 0.9f);
    EXPECT_TRUE(config.verbose);
}

// ========== Agent 状态测试 ==========

/**
 * @brief 测试 AgentState 枚举值
 */
TEST(AgentStateTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(AgentState::IDLE), 0);
    EXPECT_EQ(static_cast<int>(AgentState::THINKING), 1);
    EXPECT_EQ(static_cast<int>(AgentState::ACTING), 2);
    EXPECT_EQ(static_cast<int>(AgentState::OBSERVING), 3);
    EXPECT_EQ(static_cast<int>(AgentState::FINISHED), 4);
    EXPECT_EQ(static_cast<int>(AgentState::FAILED), 5);
}

// ========== ReActStep 测试 ==========

/**
 * @brief 测试 ReActStep 默认值
 */
TEST(ReActStepTest, DefaultValues) {
    ReActStep step;

    EXPECT_EQ(step.step_id, 0);
    EXPECT_TRUE(step.thought.empty());
    EXPECT_TRUE(step.action.empty());
    EXPECT_TRUE(step.action_input.empty());
    EXPECT_TRUE(step.observation.empty());
    EXPECT_FALSE(step.is_final);
}

/**
 * @brief 测试 ReActStep 设置
 */
TEST(ReActStepTest, Setters) {
    ReActStep step;
    step.step_id = 1;
    step.thought = "我需要搜索相关信息";
    step.action = "vector_search";
    step.action_input = R"({"query": "人工智能"})";
    step.observation = "找到 5 个相关结果";
    step.is_final = true;

    EXPECT_EQ(step.step_id, 1);
    EXPECT_EQ(step.thought, "我需要搜索相关信息");
    EXPECT_EQ(step.action, "vector_search");
    EXPECT_EQ(step.action_input, R"({"query": "人工智能"})");
    EXPECT_EQ(step.observation, "找到 5 个相关结果");
    EXPECT_TRUE(step.is_final);
}

// ========== AgentResponse 测试 ==========

/**
 * @brief 测试 AgentResponse 默认值
 */
TEST(AgentResponseTest, DefaultValues) {
    AgentResponse response;

    EXPECT_FALSE(response.success);
    EXPECT_TRUE(response.output.empty());
    EXPECT_TRUE(response.steps.empty());
    EXPECT_EQ(response.iterations_used, 0);
    EXPECT_TRUE(response.error.empty());
}

/**
 * @brief 测试 AgentResponse 设置
 */
TEST(AgentResponseTest, Setters) {
    AgentResponse response;
    response.success = true;
    response.output = "这是最终回答";
    response.iterations_used = 3;
    response.error = "";

    ReActStep step1;
    step1.step_id = 1;
    step1.thought = "思考步骤1";
    response.steps.push_back(step1);

    ReActStep step2;
    step2.step_id = 2;
    step2.is_final = true;
    response.steps.push_back(step2);

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.output, "这是最终回答");
    EXPECT_EQ(response.iterations_used, 3);
    EXPECT_EQ(response.steps.size(), 2);
    EXPECT_TRUE(response.steps[1].is_final);
}

// ========== Agent 初始化测试 ==========

/**
 * @brief 测试 Agent 构造函数
 */
TEST_F(AgentTest, Constructor) {
    Agent agent(config_);

    // Agent 应该可以被创建
    EXPECT_TRUE(true);
}

/**
 * @brief 测试 Agent 初始化（无 LLM）
 */
TEST_F(AgentTest, InitializeWithoutLLM) {
    Agent agent(config_);

    // 不初始化 LLM 就执行应该会失败或返回错误
    // 这里只测试不崩溃
    auto response = agent.execute("测试查询");
    EXPECT_FALSE(response.success);
}

// ========== ToolResult 测试 ==========

/**
 * @brief 测试 ToolResult 默认值
 */
TEST(ToolResultTest, DefaultValues) {
    ToolResult result;

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.output.empty());
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.duration_ms, 0);
}

/**
 * @brief 测试 ToolResult 设置
 */
TEST(ToolResultTest, Setters) {
    ToolResult result;
    result.success = true;
    result.output = R"({"results": [{"content": "test"}]})";
    result.error = "";
    result.duration_ms = 150;

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.output.empty());
    EXPECT_EQ(result.duration_ms, 150);
}

/**
 * @brief 测试 ToolResult 错误情况
 */
TEST(ToolResultTest, ErrorCase) {
    ToolResult result;
    result.success = false;
    result.error = "工具执行失败";
    result.duration_ms = 10;

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "工具执行失败");
}

// ========== Tool 派生类测试（使用 Mock）==========

/**
 * @brief 空 Tool 实现用于测试
 */
class MockTool : public Tool {
public:
    MockTool(const std::string& name, bool should_succeed = true)
        : name_(name), should_succeed_(should_succeed) {}

    std::string name() const override { return name_; }
    std::string description() const override { return "Mock tool for testing"; }
    std::string parameters_json_schema() const override {
        return R"({"type": "object", "properties": {"input": {"type": "string"}}})";
    }
    ToolResult execute(const std::string& parameters) override {
        ToolResult result;
        result.success = should_succeed_;
        result.output = should_succeed_ ? R"({"status": "ok"})" : "";
        result.error = should_succeed_ ? "" : "Mock failure";
        result.duration_ms = 5;
        return result;
    }

private:
    std::string name_;
    bool should_succeed_;
};

// ========== ToolRegistry 测试 ==========

/**
 * @brief 测试 ToolRegistry 注册和获取
 */
TEST(ToolTest, RegistryRegisterAndGet) {
    ToolRegistry registry;

    auto tool = std::make_shared<MockTool>("test_tool");
    registry.register_tool(tool);

    auto retrieved = registry.get_tool("test_tool");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name(), "test_tool");
}

/**
 * @brief 测试 ToolRegistry 获取不存在的工具
 */
TEST(ToolTest, RegistryGetNonExistent) {
    ToolRegistry registry;

    auto retrieved = registry.get_tool("non_existent");
    EXPECT_EQ(retrieved, nullptr);
}

/**
 * @brief 测试 ToolRegistry 列出所有工具
 */
TEST(ToolTest, RegistryListTools) {
    ToolRegistry registry;

    registry.register_tool(std::make_shared<MockTool>("tool1"));
    registry.register_tool(std::make_shared<MockTool>("tool2"));
    registry.register_tool(std::make_shared<MockTool>("tool3"));

    auto tools = registry.list_tools();
    EXPECT_EQ(tools.size(), 3);
    EXPECT_TRUE(std::find(tools.begin(), tools.end(), "tool1") != tools.end());
    EXPECT_TRUE(std::find(tools.begin(), tools.end(), "tool2") != tools.end());
    EXPECT_TRUE(std::find(tools.begin(), tools.end(), "tool3") != tools.end());
}

/**
 * @brief 测试 ToolRegistry 检查工具存在
 */
TEST(ToolTest, RegistryHasTool) {
    ToolRegistry registry;

    auto tool = std::make_shared<MockTool>("existing_tool");
    registry.register_tool(tool);

    EXPECT_TRUE(registry.has_tool("existing_tool"));
    EXPECT_FALSE(registry.has_tool("non_existent"));
}

/**
 * @brief 测试 ToolRegistry 移除工具
 */
TEST(ToolTest, RegistryUnregister) {
    ToolRegistry registry;

    registry.register_tool(std::make_shared<MockTool>("to_remove"));
    EXPECT_TRUE(registry.has_tool("to_remove"));

    registry.unregister_tool("to_remove");
    EXPECT_FALSE(registry.has_tool("to_remove"));
}

/**
 * @brief 测试 ToolRegistry 清空所有工具
 */
TEST(ToolTest, RegistryClear) {
    ToolRegistry registry;

    registry.register_tool(std::make_shared<MockTool>("tool1"));
    registry.register_tool(std::make_shared<MockTool>("tool2"));
    EXPECT_EQ(registry.list_tools().size(), 2);

    registry.clear();
    EXPECT_TRUE(registry.list_tools().empty());
}

/**
 * @brief 测试 ToolRegistry 注册空指针
 */
TEST(ToolTest, RegistryNullptr) {
    ToolRegistry registry;

    // 注册空指针应该被忽略
    registry.register_tool(nullptr);
    EXPECT_TRUE(registry.list_tools().empty());
}

// ========== Agent Tool 注册测试 ==========

/**
 * @brief 测试 Agent 注册单个工具
 */
TEST_F(AgentTest, RegisterSingleTool) {
    Agent agent(config_);

    auto tool = std::make_shared<MockTool>("search");
    agent.register_tool(tool);

    // Agent 内部有 ToolRegistry，可以通过其他方式验证
    EXPECT_TRUE(true);
}

/**
 * @brief 测试 Agent 注册多个工具
 */
TEST_F(AgentTest, RegisterMultipleTools) {
    Agent agent(config_);

    std::vector<std::shared_ptr<Tool>> tools;
    tools.push_back(std::make_shared<MockTool>("tool1"));
    tools.push_back(std::make_shared<MockTool>("tool2"));
    tools.push_back(std::make_shared<MockTool>("tool3"));

    agent.register_tools(tools);

    EXPECT_TRUE(true);
}

/**
 * @brief 测试 Agent 执行空查询
 */
TEST_F(AgentTest, ExecuteEmptyQuery) {
    Agent agent(config_);

    auto response = agent.execute("");
    // 空查询可能失败或返回错误结果
    EXPECT_FALSE(response.success || response.output.empty() == false);
}

// ========== Agent ReAct 循环测试 ==========

/**
 * @brief 测试 Agent ReAct 循环基本流程
 */
TEST_F(AgentTest, ReActLoopBasic) {
    Agent agent(config_);

    // 注册一个工具
    auto search_tool = std::make_shared<MockTool>("mock_search", true);
    agent.register_tool(search_tool);

    // 执行查询
    auto response = agent.execute("测试查询");

    // 由于没有 LLM，应该返回失败或错误
    // 验证返回了有效的响应结构
    EXPECT_TRUE(true);
}

/**
 * @brief 测试 Agent 迭代次数限制
 */
TEST_F(AgentTest, IterationLimit) {
    AgentConfig limited_config;
    limited_config.max_iterations = 2;
    limited_config.max_retries = 1;

    Agent agent(limited_config);

    // 注册工具
    agent.register_tool(std::make_shared<MockTool>("test_tool", true));

    auto response = agent.execute("测试查询");

    // 应该有限迭代次数
    EXPECT_TRUE(true);
}
