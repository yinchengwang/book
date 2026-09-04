/**
 * @file test_tool.cpp
 * @brief Modular RAG Tool 系统单元测试
 */

#include <gtest/gtest.h>
#include "rag/modular/agent/tool.h"
#include "rag/modular/config.h"
#include "rag/types.h"
#include "rag/retriever.h"
#include <memory>
#include <vector>

using namespace rag::modular::agent;

/**
 * @brief 测试夹具：Tool 基本功能测试
 */
class ToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置测试配置
        config_.default_pipeline = PipelineType::ADVANCED;
        config_.llm.model_name = "test-model";
        config_.retrieval.top_k = 5;
    }

    ModularConfig config_;
};

/**
 * @brief 测试夹具：ToolRegistry 测试
 */
class ToolRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ptr_ = std::make_shared<ToolRegistry>();
    }

    std::shared_ptr<ToolRegistry> registry_ptr_;
};

/**
 * @brief 空 Tool 实现用于测试
 */
class MockTool : public Tool {
public:
    MockTool(const std::string& tool_name, bool should_succeed = true)
        : name_(tool_name), should_succeed_(should_succeed) {}

    std::string name() const override { return name_; }
    std::string description() const override {
        return "Mock tool for testing - " + name_;
    }
    std::string parameters_json_schema() const override {
        return R"({"type": "object", "properties": {"input": {"type": "string"}}})";
    }
    ToolResult execute(const std::string& parameters) override {
        ToolResult result;
        result.success = should_succeed_;
        result.output = should_succeed_ ? R"({"status": "ok", "tool": ")" + name_ + R"("})" : "";
        result.error = should_succeed_ ? "" : "Mock failure";
        result.duration_ms = 5;
        return result;
    }

private:
    std::string name_;
    bool should_succeed_;
};

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
 * @brief 测试 ToolResult 成功情况
 */
TEST(ToolResultTest, SuccessCase) {
    ToolResult result;
    result.success = true;
    result.output = R"({"results": [{"id": "1", "content": "test content"}]})";
    result.duration_ms = 150;

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.output.empty());
    EXPECT_EQ(result.duration_ms, 150);
    EXPECT_TRUE(result.error.empty());
}

/**
 * @brief 测试 ToolResult 错误情况
 */
TEST(ToolResultTest, ErrorCase) {
    ToolResult result;
    result.success = false;
    result.error = "检索失败：索引不存在";
    result.duration_ms = 10;

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.output.empty());
    EXPECT_EQ(result.error, "检索失败：索引不存在");
}

/**
 * @brief 测试 ToolResult 长时间运行
 */
TEST(ToolResultTest, LongRunning) {
    ToolResult result;
    result.success = true;
    result.output = "处理完成";
    result.duration_ms = 5000;  // 5秒

    EXPECT_TRUE(result.success);
    EXPECT_GE(result.duration_ms, 0);
}

// ========== Tool 基类测试 ==========

/**
 * @brief 测试 Tool 接口名称
 */
TEST(ToolTest, InterfaceName) {
    auto tool = std::make_shared<MockTool>("test_tool");
    EXPECT_EQ(tool->name(), "test_tool");
}

/**
 * @brief 测试 Tool 接口描述
 */
TEST(ToolTest, InterfaceDescription) {
    auto tool = std::make_shared<MockTool>("my_tool");
    EXPECT_FALSE(tool->description().empty());
    EXPECT_TRUE(tool->description().find("my_tool") != std::string::npos);
}

/**
 * @brief 测试 Tool 参数 JSON Schema
 */
TEST(ToolTest, ParametersSchema) {
    auto tool = std::make_shared<MockTool>("schema_tool");
    std::string schema = tool->parameters_json_schema();

    EXPECT_FALSE(schema.empty());
    EXPECT_TRUE(schema.find("object") != std::string::npos);
}

/**
 * @brief 测试 Tool 执行
 */
TEST(ToolTest, Execute) {
    auto tool = std::make_shared<MockTool>("exec_tool", true);
    auto result = tool->execute(R"({"input": "test"})");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.output.empty());
}

/**
 * @brief 测试 Tool 执行失败
 */
TEST(ToolTest, ExecuteFailure) {
    auto tool = std::make_shared<MockTool>("fail_tool", false);
    auto result = tool->execute(R"({"input": "test"})");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

// ========== VectorSearchTool 测试 ==========

/**
 * @brief 测试 VectorSearchTool 名称
 */
TEST(ToolTest, VectorSearchToolName) {
    // 由于 VectorSearchTool 需要真实的 Retriever，我们只测试接口
    // 使用 Mock Retriever
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    VectorSearchTool tool(mock_retriever, 5);

    EXPECT_EQ(tool.name(), "vector_search");
}

/**
 * @brief 测试 VectorSearchTool 描述
 */
TEST(ToolTest, VectorSearchToolDescription) {
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    VectorSearchTool tool(mock_retriever, 5);

    EXPECT_FALSE(tool.description().empty());
    EXPECT_TRUE(tool.description().find("向量检索") != std::string::npos);
}

/**
 * @brief 测试 VectorSearchTool 参数 Schema
 */
TEST(ToolTest, VectorSearchToolSchema) {
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    VectorSearchTool tool(mock_retriever, 5);

    std::string schema = tool.parameters_json_schema();
    EXPECT_TRUE(schema.find("query") != std::string::npos);
    EXPECT_TRUE(schema.find("top_k") != std::string::npos);
}

/**
 * @brief 测试 VectorSearchTool 执行（空结果）
 */
TEST(ToolTest, VectorSearchToolExecute) {
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};  // 返回空结果
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    VectorSearchTool tool(mock_retriever, 5);

    auto result = tool.execute(R"({"query": "test"})");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.find("results") != std::string::npos);
}

/**
 * @brief 测试 VectorSearchTool 缺少参数
 */
TEST(ToolTest, VectorSearchToolMissingParam) {
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    VectorSearchTool tool(mock_retriever, 5);

    // 缺少必需参数 query
    auto result = tool.execute(R"({})");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

/**
 * @brief 测试 VectorSearchTool 无效 JSON
 */
TEST(ToolTest, VectorSearchToolInvalidJson) {
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    VectorSearchTool tool(mock_retriever, 5);

    auto result = tool.execute("invalid json");

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.find("JSON") != std::string::npos);
}

// ========== BM25SearchTool 测试 ==========

/**
 * @brief 测试 BM25SearchTool 名称
 */
TEST(ToolTest, BM25SearchToolName) {
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    BM25SearchTool tool(mock_retriever, 5);

    EXPECT_EQ(tool.name(), "bm25_search");
}

/**
 * @brief 测试 BM25SearchTool 描述
 */
TEST(ToolTest, BM25SearchToolDescription) {
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    BM25SearchTool tool(mock_retriever, 5);

    EXPECT_FALSE(tool.description().empty());
    EXPECT_TRUE(tool.description().find("BM25") != std::string::npos);
}

/**
 * @brief 测试 BM25SearchTool 执行
 */
TEST(ToolTest, BM25SearchToolExecute) {
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    BM25SearchTool tool(mock_retriever, 5);

    auto result = tool.execute(R"({"query": "keyword search"})");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.output.find("results") != std::string::npos);
}

/**
 * @brief 测试 BM25SearchTool 缺少参数
 */
TEST(ToolTest, BM25SearchToolMissingParam) {
    class MockRetriever : public rag::Retriever {
    public:
        std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override {
            return {};
        }
        std::string name() const override { return "mock"; }
        const rag::RetrievalConfig& config() const override { return config_; }
        rag::RetrievalConfig config_;
    };

    auto mock_retriever = std::make_shared<MockRetriever>();
    BM25SearchTool tool(mock_retriever, 5);

    auto result = tool.execute(R"({})");

    EXPECT_FALSE(result.success);
}

// ========== ToolRegistry 测试 ==========

/**
 * @brief 测试 ToolRegistry 创建
 */
TEST(ToolRegistryTest, Create) {
    EXPECT_NE(registry_ptr_, nullptr);
}

/**
 * @brief 测试 ToolRegistry 注册工具
 */
TEST(ToolRegistryTest, RegisterTool) {
    auto tool = std::make_shared<MockTool>("register_test");
    registry_ptr_->register_tool(tool);

    auto retrieved = registry_ptr_->get_tool("register_test");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name(), "register_test");
}

/**
 * @brief 测试 ToolRegistry 获取不存在的工具
 */
TEST(ToolRegistryTest, GetNonExistent) {
    auto retrieved = registry_ptr_->get_tool("non_existent_tool");
    EXPECT_EQ(retrieved, nullptr);
}

/**
 * @brief 测试 ToolRegistry 列出所有工具
 */
TEST(ToolRegistryTest, ListTools) {
    registry_ptr_->register_tool(std::make_shared<MockTool>("list_tool1"));
    registry_ptr_->register_tool(std::make_shared<MockTool>("list_tool2"));

    auto tools = registry_ptr_->list_tools();
    EXPECT_EQ(tools.size(), 2);
}

/**
 * @brief 测试 ToolRegistry 检查工具存在
 */
TEST(ToolRegistryTest, HasTool) {
    registry_ptr_->register_tool(std::make_shared<MockTool>("has_tool"));

    EXPECT_TRUE(registry_ptr_->has_tool("has_tool"));
    EXPECT_FALSE(registry_ptr_->has_tool("not_exists"));
}

/**
 * @brief 测试 ToolRegistry 移除工具
 */
TEST(ToolRegistryTest, UnregisterTool) {
    registry_ptr_->register_tool(std::make_shared<MockTool>("unreg_tool"));
    EXPECT_TRUE(registry_ptr_->has_tool("unreg_tool"));

    registry_ptr_->unregister_tool("unreg_tool");
    EXPECT_FALSE(registry_ptr_->has_tool("unreg_tool"));
}

/**
 * @brief 测试 ToolRegistry 清空所有工具
 */
TEST(ToolRegistryTest, ClearAll) {
    registry_ptr_->register_tool(std::make_shared<MockTool>("clear1"));
    registry_ptr_->register_tool(std::make_shared<MockTool>("clear2"));
    registry_ptr_->register_tool(std::make_shared<MockTool>("clear3"));

    EXPECT_EQ(registry_ptr_->list_tools().size(), 3);

    registry_ptr_->clear();

    EXPECT_TRUE(registry_ptr_->list_tools().empty());
}

/**
 * @brief 测试 ToolRegistry 注册空指针
 */
TEST(ToolRegistryTest, RegisterNullptr) {
    registry_ptr_->register_tool(nullptr);
    EXPECT_TRUE(registry_ptr_->list_tools().empty());
}

/**
 * @brief 测试 ToolRegistry 重复注册
 */
TEST(ToolRegistryTest, DuplicateRegister) {
    registry_ptr_->register_tool(std::make_shared<MockTool>("dup_tool"));
    registry_ptr_->register_tool(std::make_shared<MockTool>("dup_tool"));  // 覆盖

    EXPECT_EQ(registry_ptr_->list_tools().size(), 1);
}

/**
 * @brief 测试 ToolRegistry 多次获取
 */
TEST(ToolRegistryTest, MultipleGet) {
    registry_ptr_->register_tool(std::make_shared<MockTool>("multi_get"));

    auto tool1 = registry_ptr_->get_tool("multi_get");
    auto tool2 = registry_ptr_->get_tool("multi_get");

    EXPECT_NE(tool1, nullptr);
    EXPECT_NE(tool2, nullptr);
    EXPECT_EQ(tool1->name(), tool2->name());
}

// ========== 工具执行性能测试 ==========

/**
 * @brief 测试工具执行时间记录
 */
TEST(ToolTest, ExecuteTiming) {
    auto tool = std::make_shared<MockTool>("timing_tool", true);
    auto result = tool->execute(R"({"input": "performance test"})");

    EXPECT_TRUE(result.success);
    EXPECT_GE(result.duration_ms, 0);
}

/**
 * @brief 测试连续多次执行
 */
TEST(ToolTest, MultipleExecute) {
    auto tool = std::make_shared<MockTool>("multi_exec", true);

    for (int i = 0; i < 10; ++i) {
        auto result = tool->execute(R"({"input": "test"})");
        EXPECT_TRUE(result.success);
    }
}
