/**
 * @file tool.h
 * @brief Agent Tool 系统 - 工具接口和注册表
 */
#pragma once

#include "rag/types.h"
#include "rag/retriever.h"
#include "rag/graph_retriever.h"
#include "rag/llm_service.h"
#include "rag/modular/config.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>

namespace rag::modular::agent {

// ========== 工具结果 ==========

/**
 * @brief 工具执行结果
 */
struct ToolResult {
    bool success = false;              // 是否成功
    std::string output;                // 输出内容
    std::string error;                 // 错误信息
    int64_t duration_ms = 0;           // 执行耗时（毫秒）
};

// ========== 工具接口 ==========

/**
 * @brief 工具接口
 *
 * 所有 Agent 工具都继承此接口
 */
class Tool {
public:
    virtual ~Tool() = default;

    /// 获取工具名称
    virtual std::string name() const = 0;

    /// 获取工具描述
    virtual std::string description() const = 0;

    /// 获取参数的 JSON Schema
    virtual std::string parameters_json_schema() const = 0;

    /// 执行工具
    virtual ToolResult execute(const std::string& parameters) = 0;
};

// ========== 向量搜索工具 ==========

/**
 * @brief 向量搜索工具
 *
 * 使用向量检索从知识库中搜索相关文本块
 */
class VectorSearchTool : public Tool {
public:
    /**
     * @brief 构造函数
     * @param retriever 向量检索器
     * @param top_k 返回结果数量
     */
    VectorSearchTool(std::shared_ptr<rag::Retriever> retriever, int top_k = 5);

    std::string name() const override { return "vector_search"; }
    std::string description() const override;
    std::string parameters_json_schema() const override;
    ToolResult execute(const std::string& parameters) override;

private:
    std::shared_ptr<rag::Retriever> retriever_;
    int top_k_;
};

// ========== BM25 搜索工具 ==========

/**
 * @brief BM25 全文搜索工具
 *
 * 使用 BM25 算法从知识库中搜索相关文本块
 */
class BM25SearchTool : public Tool {
public:
    /**
     * @brief 构造函数
     * @param retriever BM25 检索器
     * @param top_k 返回结果数量
     */
    BM25SearchTool(std::shared_ptr<rag::Retriever> retriever, int top_k = 5);

    std::string name() const override { return "bm25_search"; }
    std::string description() const override;
    std::string parameters_json_schema() const override;
    ToolResult execute(const std::string& parameters) override;

private:
    std::shared_ptr<rag::Retriever> retriever_;
    int top_k_;
};

// ========== Graph 搜索工具 ==========

/**
 * @brief Graph 搜索工具
 *
 * 使用知识图谱检索相关实体和关系
 */
class GraphSearchTool : public Tool {
public:
    /**
     * @brief 构造函数
     * @param graph_retriever Graph 检索器
     * @param top_k 返回结果数量
     */
    GraphSearchTool(std::shared_ptr<rag::GraphRetriever> graph_retriever, int top_k = 5);

    std::string name() const override { return "graph_search"; }
    std::string description() const override;
    std::string parameters_json_schema() const override;
    ToolResult execute(const std::string& parameters) override;

private:
    std::shared_ptr<rag::GraphRetriever> graph_retriever_;
    int top_k_;
};

// ========== LLM 生成工具 ==========

/**
 * @brief LLM 生成工具
 *
 * 使用 LLM 生成回答
 */
class LLMGenerateTool : public Tool {
public:
    /**
     * @brief 构造函数
     * @param llm_service LLM 服务
     */
    LLMGenerateTool(std::shared_ptr<rag::LLMService> llm_service);

    std::string name() const override { return "llm_generate"; }
    std::string description() const override;
    std::string parameters_json_schema() const override;
    ToolResult execute(const std::string& parameters) override;

private:
    std::shared_ptr<rag::LLMService> llm_service_;
};

// ========== 工具注册表 ==========

/**
 * @brief 工具注册表
 *
 * 管理所有可用的工具，支持注册、获取和列表操作
 */
class ToolRegistry {
public:
    /// 注册工具
    void register_tool(std::shared_ptr<Tool> tool);

    /// 获取工具
    std::shared_ptr<Tool> get_tool(const std::string& name);

    /// 列出所有工具名称
    std::vector<std::string> list_tools() const;

    /// 检查工具是否存在
    bool has_tool(const std::string& name) const;

    /// 移除工具
    void unregister_tool(const std::string& name);

    /// 清空所有工具
    void clear();

private:
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};

}  // namespace rag::modular::agent
