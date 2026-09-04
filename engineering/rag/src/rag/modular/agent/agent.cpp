/**
 * @file agent.cpp
 * @brief Agent 核心实现 - ReAct 推理代理
 */

#include "rag/modular/agent/agent.h"
#include "rag/error.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

namespace rag::modular::agent {

// ========== 构造函数 ==========

Agent::Agent(const AgentConfig& config)
    : config_(config), tools_(std::make_shared<ToolRegistry>()) {
}

// ========== 初始化 ==========

void Agent::initialize(std::shared_ptr<rag::LLMService> llm) {
    llm_ = std::move(llm);
}

// ========== 工具注册 ==========

void Agent::register_tool(std::shared_ptr<Tool> tool) {
    if (tool) {
        tools_->register_tool(std::move(tool));
    }
}

void Agent::register_tools(const std::vector<std::shared_ptr<Tool>>& tools) {
    for (const auto& tool : tools) {
        if (tool) {
            tools_->register_tool(tool);
        }
    }
}

// ========== 执行查询 ==========

AgentResponse Agent::execute(const std::string& query) {
    AgentResponse response;
    std::vector<ReActStep> history;

    if (!llm_) {
        response.error = "LLM service not initialized";
        return response;
    }

    if (config_.verbose) {
        std::cout << "[Agent] Starting execution with query: " << query << std::endl;
    }

    // ReAct 迭代循环
    for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
        if (config_.verbose) {
            std::cout << "[Agent] Iteration " << (iteration + 1) << "/" << config_.max_iterations << std::endl;
        }

        // Step 1: Think - 使用 LLM 生成推理步骤
        ReActStep step = think(query, history);
        step.step_id = iteration + 1;

        if (config_.verbose) {
            std::cout << "[Agent] Thought: " << step.thought << std::endl;
            std::cout << "[Agent] Action: " << step.action << std::endl;
        }

        // 检查是否为最终答案
        if (step.is_final || step.action == "finish") {
            step.observation = "Final answer generated";
            history.push_back(step);
            response.success = true;
            response.output = step.action_input;
            response.steps = history;
            response.iterations_used = iteration + 1;

            if (config_.verbose) {
                std::cout << "[Agent] Finished with answer: " << response.output << std::endl;
            }
            return response;
        }

        // Step 2: Act - 执行动作
        if (!step.action.empty() && step.action != "None") {
            ToolResult tool_result = act(step.action, step.action_input);
            step.observation = tool_result.success
                ? tool_result.output
                : std::string("Error: ") + tool_result.error;

            if (config_.verbose) {
                std::cout << "[Agent] Observation: " << step.observation << std::endl;
            }
        }

        history.push_back(step);

        // 检查是否达到最大迭代次数
        if (iteration + 1 >= config_.max_iterations) {
            response.error = "Max iterations reached";
            break;
        }
    }

    // 未能得到最终答案
    response.success = false;
    response.steps = history;
    response.iterations_used = config_.max_iterations;
    response.error = response.error.empty() ? "Agent execution failed" : response.error;

    return response;
}

// ========== 思考步骤 ==========

ReActStep Agent::think(const std::string& query, const std::vector<ReActStep>& history) {
    ReActStep step;

    // 构建提示词
    std::ostringstream prompt;
    prompt << "You are a helpful AI assistant with access to tools.\n\n";

    // 添加可用工具描述
    prompt << "Available tools:\n";
    for (const auto& tool_name : tools_->list_tools()) {
        auto tool = tools_->get_tool(tool_name);
        if (tool) {
            prompt << "- " << tool->name() << ": " << tool->description() << "\n";
            prompt << "  Parameters: " << tool->parameters_json_schema() << "\n";
        }
    }
    prompt << "\n";

    // 添加历史上下文
    if (!history.empty()) {
        prompt << "Previous steps:\n";
        for (const auto& h : history) {
            prompt << "Step " << h.step_id << ":\n";
            prompt << "  Thought: " << h.thought << "\n";
            prompt << "  Action: " << h.action << "\n";
            prompt << "  Action Input: " << h.action_input << "\n";
            prompt << "  Observation: " << h.observation << "\n\n";
        }
    }

    // 添加当前查询
    prompt << "Current query: " << query << "\n\n";

    // 添加输出格式说明
    prompt << R"JSON(
Output your response in JSON format with the following fields:
{
    "thought": "your reasoning about what to do next",
    "action": "the tool name to use (or 'finish' to end)",
    "action_input": "the parameters for the tool as a JSON string, or the final answer if finishing",
    "is_final": true/false
}

Examples:
- To search for information: {"thought": "I need to search for...", "action": "vector_search", "action_input": "{\"query\": \"search terms\"}", "is_final": false}
- To finish: {"thought": "I have enough information", "action": "finish", "action_input": "The final answer is...", "is_final": true}
)JSON";

    // 调用 LLM 生成
    rag::GenerateOptions options;
    options.max_tokens = 1024;
    options.temperature = config_.temperature;
    options.stop = "";

    try {
        auto result = llm_->generate(prompt.str(), options);

        // 解析 LLM 输出
        std::string llm_output = result.text;

        // 尝试提取 JSON
        size_t json_start = llm_output.find('{');
        size_t json_end = llm_output.find('}');

        if (json_start != std::string::npos && json_end != std::string::npos) {
            std::string json_str = llm_output.substr(json_start, json_end - json_start + 1);
            json parsed = json::parse(json_str);

            step.thought = parsed.value("thought", "");
            step.action = parsed.value("action", "");
            step.action_input = parsed.value("action_input", "");
            step.is_final = parsed.value("is_final", false);
        } else {
            // 如果无法解析 JSON，使用原始输出作为思考
            step.thought = llm_output;
            step.action = "";
            step.action_input = "";
            step.is_final = false;
        }

    } catch (const std::exception& e) {
        step.thought = std::string("Error generating response: ") + e.what();
        step.action = "";
        step.action_input = "";
        step.is_final = false;
    }

    return step;
}

// ========== 执行步骤 ==========

ToolResult Agent::act(const std::string& action, const std::string& params) {
    if (action.empty() || action == "None") {
        return {true, "", ""};
    }

    auto tool = tools_->get_tool(action);
    if (!tool) {
        return {false, "", "Tool not found: " + action};
    }

    // 重试机制
    ToolResult last_result;
    for (int retry = 0; retry < config_.max_retries; ++retry) {
        last_result = tool->execute(params);

        if (last_result.success) {
            return last_result;
        }

        if (retry < config_.max_retries - 1) {
            if (config_.verbose) {
                std::cout << "[Agent] Retrying tool execution (attempt " << (retry + 2)
                          << "/" << config_.max_retries << ")" << std::endl;
            }
        }
    }

    return last_result;
}

}  // namespace rag::modular::agent
