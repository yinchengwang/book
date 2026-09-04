/**
 * @file react_pipeline.cpp
 * @brief ReActPipeline 实现
 *
 * 流程: Query → Agent循环(Thought→Action→Observation) → LLM
 */

#define RAG_AGENT_AVAILABLE

#include "rag/modular/pipeline/react_pipeline.h"
#include "rag/modular/agent/agent.h"
#include "rag/modular/agent/tool.h"
#include "rag/logger.h"
#include <chrono>
#include <algorithm>
#include <cctype>

namespace rag::modular {

namespace {

// 辅助函数：去除字符串首尾空白
std::string trim_string(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

// 辅助函数：转小写
std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

}  // anonymous namespace

ReActPipeline::ReActPipeline()
    : initialized_(false),
      max_iterations_(5),
      current_iteration_(0),
      satisfaction_threshold_(0.6f) {
}

ReActPipeline::~ReActPipeline() = default;

bool ReActPipeline::init(const ModularConfig& config) {
    RAG_INFO("初始化 ReActPipeline...");

    // 保存配置
    config_ = config;

    // 初始化 LLM 服务
    if (!config.llm.model_path.empty()) {
        llm_ = rag::create_llm_service();
        if (llm_) {
            llm_->load(config.llm.model_path, config.llm);
            RAG_INFO("LLM 模型加载完成: " + config.llm.model_path);
        } else {
            RAG_WARN("LLM 服务创建失败");
        }
    }

#ifdef RAG_AGENT_AVAILABLE
    // Task 8: 创建真实 Agent 实例
    rag::modular::agent::AgentConfig agent_config;
    agent_config.max_iterations = max_iterations_;
    agent_config.max_retries = 3;
    agent_config.temperature = config.llm.temperature;
    agent_config.verbose = false;

    agent_ = std::make_unique<rag::modular::agent::Agent>(agent_config);

    // 初始化 Agent
    if (llm_) {
        agent_->initialize(llm_);
    }

    // 注册 Tools
    if (hnsw_retriever_) {
        auto vector_tool = std::make_shared<rag::modular::agent::VectorSearchTool>(
            hnsw_retriever_, config.retrieval.top_k);
        agent_->register_tool(vector_tool);
    }

    if (bm25_retriever_) {
        auto bm25_tool = std::make_shared<rag::modular::agent::BM25SearchTool>(
            bm25_retriever_, config.retrieval.top_k);
        agent_->register_tool(bm25_tool);
    }

    RAG_INFO("Agent 实例创建并注册 Tools 完成");
#else
    // 使用占位符 Agent (简单基于规则的实现)
    RAG_INFO("ReActPipeline 使用占位符 Agent (Task 8 待实现)");
#endif

    initialized_ = true;
    RAG_INFO("ReActPipeline 初始化完成");
    return true;
}

bool ReActPipeline::is_ready() const {
    return initialized_ && llm_ && llm_->is_loaded()
           && (hnsw_retriever_ || bm25_retriever_);
}

ModularQueryResult ReActPipeline::query(const ModularQuery& query) {
    ModularQueryResult result;
    auto start_time = std::chrono::steady_clock::now();

    // 检查是否就绪
    if (!is_ready()) {
        result.error_message = "Pipeline 未就绪，请先调用 init() 或设置必要的检索器";
        RAG_ERROR(result.error_message);
        return result;
    }

    // 重置状态
    reset();

    // 初始化查询状态
    state_.original_query = query.text;
    state_.current_query = query.text;

    int top_k = query.top_k > 0 ? query.top_k : config_.retrieval.top_k;

    RAG_INFO("ReActPipeline 开始处理查询: " + query.text);

#ifdef RAG_AGENT_AVAILABLE
    // Task 8: 使用真实 Agent 执行查询
    if (agent_) {
        auto agent_response = agent_->execute(query.text);

        if (agent_response.success) {
            result.answer = agent_response.output;

            // 转换 Agent 的 ReActStep 到 pipeline 的 ReActStep
            for (const auto& agent_step : agent_response.steps) {
                ReActStep step;
                step.step_number = agent_step.step_id;
                step.thought = agent_step.thought;
                step.action_input = agent_step.action_input;
                step.observation = agent_step.observation;
                step.is_final = agent_step.is_final;

                // 解析 action 字符串为 ReActAction
                if (agent_step.action == "finish" || agent_step.action == "generate") {
                    step.action = ReActAction::FINISH;
                } else if (agent_step.action == "refine_query") {
                    step.action = ReActAction::REFINE_QUERY;
                } else if (agent_step.action == "expand_context") {
                    step.action = ReActAction::EXPAND_CONTEXT;
                } else {
                    step.action = ReActAction::RETRIEVE;
                }

                state_.steps.push_back(step);
            }

            stats_.total_iterations = agent_response.iterations_used;
            result.success = true;
        } else {
            result.error_message = agent_response.error;
            result.success = false;
        }
    } else {
        // Agent 未初始化，回退到占位符实现
        run_react_loop();
        result.answer = build_final_answer();
    }
#else
    // 使用占位符实现
    run_react_loop();
    result.answer = build_final_answer();
#endif

    result.context = state_.all_results;

    // 计算耗时
    result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();

    if (result.success) {
        RAG_INFO("ReActPipeline 查询完成，迭代: " + std::to_string(stats_.total_iterations) +
                    "，检索次数: " + std::to_string(stats_.retrieve_count) +
                    "，精炼次数: " + std::to_string(stats_.refine_count));
    }

    return result;
}

void ReActPipeline::set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever) {
    hnsw_retriever_ = retriever;
}

void ReActPipeline::set_bm25_retriever(std::shared_ptr<rag::BM25Retriever> retriever) {
    bm25_retriever_ = retriever;
}

void ReActPipeline::reset() {
    state_ = ReActState();
    current_iteration_ = 0;
    stats_ = Stats();
}

std::vector<rag::RetrievalResult> ReActPipeline::retrieve(
    const std::string& query, int top_k) {
    std::vector<rag::RetrievalResult> results;

    // HNSW 检索
    if (hnsw_retriever_) {
        try {
            auto hnsw_results = hnsw_retriever_->retrieve(query, top_k);
            results.insert(results.end(), hnsw_results.begin(), hnsw_results.end());
        } catch (const std::exception& e) {
            RAG_ERROR(std::string("HNSW 检索异常: ") + e.what());
        }
    }

    // BM25 检索
    if (bm25_retriever_) {
        try {
            auto bm25_results = bm25_retriever_->retrieve(query, top_k);
            results.insert(results.end(), bm25_results.begin(), bm25_results.end());
        } catch (const std::exception& e) {
            RAG_ERROR(std::string("BM25 检索异常: ") + e.what());
        }
    }

    // 按分数排序并去重
    std::sort(results.begin(), results.end(),
              [](const rag::RetrievalResult& a, const rag::RetrievalResult& b) {
                  return a.score > b.score;
              });

    std::vector<rag::RetrievalResult> unique_results;
    std::unordered_set<std::string> seen_ids;
    for (const auto& r : results) {
        if (seen_ids.find(r.chunk.id) == seen_ids.end()) {
            seen_ids.insert(r.chunk.id);
            unique_results.push_back(r);
        }
    }

    stats_.retrieve_count++;
    return unique_results;
}

void ReActPipeline::run_react_loop() {
    for (current_iteration_ = 0; current_iteration_ < max_iterations_; ++current_iteration_) {
        stats_.total_iterations++;

        RAG_INFO("ReActPipeline 迭代 " + std::to_string(current_iteration_ + 1) +
                    "，当前查询: " + state_.current_query);

        // 决定下一步动作
        ReActAction next_action = decide_next_action();

        // 生成思考
        ReActStep step;
        step.step_number = current_iteration_ + 1;
        step.action = next_action;
        step.thought = generate_thought(next_action);

        RAG_INFO("ReAct 步骤 " + std::to_string(step.step_number) +
                    " - 思考: " + step.thought.substr(0, 100) + "...");

        // 执行动作
        step.action_input = state_.current_query;
        step.observation = execute_action(next_action);

        RAG_INFO("ReAct 步骤 " + std::to_string(step.step_number) +
                    " - 观察: " + step.observation.substr(0, 100) + "...");

        state_.steps.push_back(step);

        // 检查是否满足
        if (is_satisfied() || next_action == ReActAction::FINISH) {
            RAG_INFO("ReAct 循环满足条件，退出");
            break;
        }
    }
}

ReActAction ReActPipeline::decide_next_action() {
    // 如果没有检索结果，首先检索
    if (state_.all_results.empty()) {
        return ReActAction::RETRIEVE;
    }

    // 如果达到最大迭代，生成回答
    if (current_iteration_ >= max_iterations_ - 1) {
        return ReActAction::FINISH;
    }

    // 基于规则的决定 (占位符实现)
    // Task 8 Agent 实现后替换为 agent_->decide_action(state_)

    // 计算当前上下文的平均分数
    float avg_score = 0.0f;
    if (!state_.all_results.empty()) {
        float sum = 0.0f;
        for (const auto& r : state_.all_results) {
            sum += r.score;
        }
        avg_score = sum / state_.all_results.size();
    }

    // 如果平均分数足够高，生成回答
    if (avg_score >= satisfaction_threshold_) {
        return ReActAction::FINISH;
    }

    // 否则扩展上下文或精炼查询
    if (current_iteration_ % 2 == 0) {
        return ReActAction::EXPAND_CONTEXT;
    } else {
        return ReActAction::REFINE_QUERY;
    }
}

std::string ReActPipeline::generate_thought(ReActAction action) {
    std::ostringstream oss;

    switch (action) {
        case ReActAction::RETRIEVE:
            oss << "我需要检索与查询相关的文档。";
            break;
        case ReActAction::REFINE_QUERY:
            oss << "当前检索结果不够充分，我需要优化查询。";
            break;
        case ReActAction::EXPAND_CONTEXT:
            oss << "我需要扩展当前上下文以获得更多信息。";
            break;
        case ReActAction::GENERATE:
            oss << "我有足够的信息来生成回答了。";
            break;
        case ReActAction::FINISH:
            oss << "我已经收集了足够的信息，现在生成最终回答。";
            break;
    }

    // 添加当前状态上下文
    oss << " 当前已检索 " << state_.all_results.size() << " 个结果，";
    oss << "迭代 " << (current_iteration_ + 1) << "/" << max_iterations_ << "。";

    return oss.str();
}

std::string ReActPipeline::execute_action(ReActAction action) {
    int top_k = config_.retrieval.top_k;

    switch (action) {
        case ReActAction::RETRIEVE:
        case ReActAction::EXPAND_CONTEXT: {
            auto results = retrieve(state_.current_query, top_k);
            if (results.empty()) {
                return "未找到相关文档";
            }

            // 添加到所有结果中
            for (const auto& r : results) {
                bool found = false;
                for (const auto& existing : state_.all_results) {
                    if (existing.chunk.id == r.chunk.id) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    state_.all_results.push_back(r);
                }
            }

            std::ostringstream oss;
            oss << "检索到 " << results.size() << " 个相关文档";
            if (action == ReActAction::EXPAND_CONTEXT) {
                oss << "，总共有 " << state_.all_results.size() << " 个上下文";
            }
            return oss.str();
        }

        case ReActAction::REFINE_QUERY: {
            // 使用 LLM 优化查询
            if (!llm_ || !llm_->is_loaded()) {
                return "LLM 不可用，无法优化查询";
            }

            std::ostringstream prompt;
            prompt << "请根据以下对话历史，优化查询以获得更好的检索结果。\n\n"
                   << "原始查询: " << state_.original_query << "\n\n"
                   << "当前查询: " << state_.current_query << "\n\n"
                   << "已检索到的上下文摘要:\n";

            // 限制上下文长度
            size_t context_len = 0;
            for (const auto& r : state_.all_results) {
                context_len += r.chunk.content.size();
                if (context_len > 500) break;
                prompt << "- " << r.chunk.content.substr(0, 200) << "\n\n";
            }

            prompt << "\n请生成一个优化后的查询（只返回查询文本，不要其他内容）:";

            try {
                GenerateOptions options;
                options.max_tokens = 128;
                options.temperature = 0.7f;

                auto gen_result = llm_->generate(prompt.str(), options);
                if (gen_result.finished && !gen_result.text.empty()) {
                    std::string new_query = trim_string(gen_result.text);
                    if (!new_query.empty() && new_query != state_.current_query) {
                        state_.current_query = new_query;
                        stats_.refine_count++;
                        RAG_INFO("查询优化: " + state_.current_query);
                        return "查询已优化为: " + new_query;
                    }
                }
            } catch (const std::exception& e) {
                RAG_ERROR(std::string("查询优化异常: ") + e.what());
            }

            return "查询优化未产生变化";
        }

        case ReActAction::GENERATE:
        case ReActAction::FINISH:
            return "准备生成最终回答";

        default:
            return "未知动作";
    }
}

bool ReActPipeline::is_satisfied() const {
    // 如果没有结果，不满足
    if (state_.all_results.empty()) {
        return false;
    }

    // 如果上下文数量足够，认为满足
    if (static_cast<int>(state_.all_results.size()) >= config_.retrieval.top_k) {
        return true;
    }

    // 计算平均分数
    float avg_score = 0.0f;
    for (const auto& r : state_.all_results) {
        avg_score += r.score;
    }
    avg_score /= state_.all_results.size();

    return avg_score >= satisfaction_threshold_;
}

std::string ReActPipeline::build_final_answer() {
    if (state_.all_results.empty()) {
        return "抱歉，未找到与您查询相关的文档内容。";
    }

    // 构建上下文字符串
    std::string context_str = build_context(state_.original_query, state_.all_results);

    // 构建生成提示词
    std::ostringstream prompt;
    prompt << "你是一个知识问答助手。请根据以下上下文信息回答问题。\n\n"
           << "问题: " << state_.original_query << "\n\n"
           << "上下文:\n" << context_str << "\n\n"
           << "请生成一个准确、完整的回答。\n\n"
           << "回答:";

    try {
        GenerateOptions options;
        options.max_tokens = config_.llm.max_tokens;
        options.temperature = config_.llm.temperature;

        return generate_with_llm(prompt.str(), options);
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("生成回答异常: ") + e.what());
        return "生成回答时发生错误。";
    }
}

std::string ReActPipeline::build_thought_prompt(const ReActState& state, ReActAction next_action) {
    std::ostringstream oss;
    oss << "你是一个推理助手，正在分析查询并决定下一步行动。\n\n"
        << "查询: " << state.current_query << "\n\n"
        << "已执行步骤:\n";

    for (const auto& step : state.steps) {
        oss << step.step_number << ". "
            << "动作: " << static_cast<int>(step.action) << ", "
            << "思考: " << step.thought << "\n"
            << "   观察: " << step.observation << "\n\n";
    }

    oss << "当前状态:\n"
        << "- 已检索文档数: " << state.all_results.size() << "\n"
        << "- 当前迭代: " << (current_iteration_ + 1) << "/" << max_iterations_ << "\n\n";

    oss << "请决定下一步动作（只返回一个数字）:\n"
        << "0: RETRIEVE (检索)\n"
        << "1: GENERATE (生成回答)\n"
        << "2: REFINE_QUERY (优化查询)\n"
        << "3: EXPAND_CONTEXT (扩展上下文)\n"
        << "4: FINISH (结束)\n\n"
        << "你的决定:";

    return oss.str();
}

ReActAction ReActPipeline::parse_action_decision(const std::string& response) {
    std::string trimmed = trim_string(response);
    if (trimmed.empty()) return ReActAction::RETRIEVE;

    // 尝试提取数字
    try {
        int action = std::stoi(trimmed);
        if (action >= 0 && action <= 4) {
            return static_cast<ReActAction>(action);
        }
    } catch (...) {
        // 解析失败，使用默认逻辑
    }

    // 简单关键词匹配
    std::string lower = to_lower(trimmed);
    if (lower.find("finish") != std::string::npos || lower.find("generate") != std::string::npos) {
        return ReActAction::FINISH;
    }
    if (lower.find("refine") != std::string::npos || lower.find("rewrite") != std::string::npos) {
        return ReActAction::REFINE_QUERY;
    }
    if (lower.find("expand") != std::string::npos) {
        return ReActAction::EXPAND_CONTEXT;
    }

    return ReActAction::RETRIEVE;
}

}  // namespace rag::modular
