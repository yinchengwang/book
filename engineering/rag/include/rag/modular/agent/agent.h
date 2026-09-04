/**
 * @file agent.h
 * @brief Agent 核心 - ReAct 推理代理
 */
#pragma once

#include "rag/modular/agent/tool.h"
#include "rag/llm_service.h"
#include <string>
#include <vector>
#include <memory>

namespace rag::modular::agent {

// ========== 配置结构 ==========

/**
 * @brief Agent 配置
 */
struct AgentConfig {
    int max_iterations = 10;       // 最大迭代次数
    int max_retries = 3;           // 最大重试次数
    float temperature = 0.0f;      // 生成温度
    bool verbose = false;          // 是否输出详细信息
};

// ========== 状态枚举 ==========

/**
 * @brief Agent 状态
 */
enum class AgentState { IDLE, THINKING, ACTING, OBSERVING, FINISHED, FAILED };

// ========== ReAct 步骤 ==========

/**
 * @brief ReAct 推理步骤
 */
struct ReActStep {
    int step_id;                   // 步骤编号
    std::string thought;           // 思考过程
    std::string action;            // 执行的动作
    std::string action_input;      // 动作输入参数
    std::string observation;       // 观察结果
    bool is_final = false;         // 是否为最终步骤
};

// ========== Agent 响应 ==========

/**
 * @brief Agent 执行响应
 */
struct AgentResponse {
    bool success = false;          // 是否成功
    std::string output;            // 最终输出
    std::vector<ReActStep> steps;  // 执行步骤历史
    int iterations_used = 0;       // 使用的迭代次数
    std::string error;             // 错误信息
};

// ========== Agent 类 ==========

/**
 * @brief ReAct 推理代理
 *
 * 实现 ReAct (Reasoning + Acting) 模式的智能代理
 * 支持多工具调用、思考过程记录和迭代推理
 */
class Agent {
public:
    /**
     * @brief 构造函数
     * @param config Agent 配置
     */
    explicit Agent(const AgentConfig& config);

    /**
     * @brief 初始化 Agent
     * @param llm LLM 服务
     */
    void initialize(std::shared_ptr<rag::LLMService> llm);

    /**
     * @brief 注册单个工具
     * @param tool 要注册的工具
     */
    void register_tool(std::shared_ptr<Tool> tool);

    /**
     * @brief 注册多个工具
     * @param tools 要注册的工具列表
     */
    void register_tools(const std::vector<std::shared_ptr<Tool>>& tools);

    /**
     * @brief 执行查询
     * @param query 用户查询
     * @return Agent 响应
     */
    AgentResponse execute(const std::string& query);

private:
    /**
     * @brief 思考步骤 - 使用 LLM 生成下一步推理
     * @param query 原始查询
     * @param history 推理历史
     * @return ReAct 步骤
     */
    ReActStep think(const std::string& query, const std::vector<ReActStep>& history);

    /**
     * @brief 执行步骤 - 调用工具执行动作
     * @param action 动作名称
     * @param params 动作参数 (JSON 格式)
     * @return 工具执行结果
     */
    ToolResult act(const std::string& action, const std::string& params);

    AgentConfig config_;                                // Agent 配置
    std::shared_ptr<rag::LLMService> llm_;             // LLM 服务
    std::shared_ptr<ToolRegistry> tools_;              // 工具注册表
};

}  // namespace rag::modular::agent
