/**
 * @file naive_pipeline.cpp
 * @brief NaivePipeline 实现
 *
 * 流程: Query → Vector检索 → Context → LLM
 */

#include "rag/modular/pipeline/naive_pipeline.h"
#include "rag/logger.h"
#include <chrono>

namespace rag::modular {

NaivePipeline::NaivePipeline() : initialized_(false) {}

NaivePipeline::~NaivePipeline() = default;

bool NaivePipeline::init(const ModularConfig& config) {
    RAG_INFO("初始化 NaivePipeline...");

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

    initialized_ = true;
    RAG_INFO("NaivePipeline 初始化完成");
    return true;
}

bool NaivePipeline::is_ready() const {
    return initialized_ && llm_ && llm_->is_loaded() && hnsw_retriever_;
}

ModularQueryResult NaivePipeline::query(const ModularQuery& query) {
    ModularQueryResult result;
    auto start_time = std::chrono::steady_clock::now();

    // 检查是否就绪
    if (!is_ready()) {
        result.error_message = "Pipeline 未就绪，请先调用 init()";
        RAG_ERROR(result.error_message);
        return result;
    }

    // 执行向量检索
    int top_k = query.top_k > 0 ? query.top_k : config_.retrieval.top_k;
    auto retrieval_start = std::chrono::steady_clock::now();
    auto retrieval_results = retrieve_vectors(query.text, top_k);
    result.retrieval_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - retrieval_start).count();

    if (retrieval_results.empty()) {
        RAG_WARN("检索结果为空: " + query.text);
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    // 保存检索结果
    result.context = retrieval_results;

    // 构建上下文字符串
    std::string context_str = build_context(query.text, retrieval_results);

    // 构建提示词
    std::string prompt = "请根据以下上下文信息回答问题。如果上下文中没有相关信息，请说明无法回答。\n\n"
                       "问题: " + query.text + "\n\n"
                       "上下文:\n" + context_str + "\n\n"
                       "回答:";

    // 调用 LLM 生成回答
    auto gen_start = std::chrono::steady_clock::now();
    GenerateOptions options;
    options.max_tokens = config_.llm.max_tokens;
    options.temperature = config_.llm.temperature;

    result.answer = generate_with_llm(prompt, options);
    result.generation_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - gen_start).count();

    result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    result.success = true;

    RAG_INFO("NaivePipeline 查询完成，检索: " + std::to_string(result.retrieval_time_ms) +
                "ms, 生成: " + std::to_string(result.generation_time_ms) + "ms");

    return result;
}

void NaivePipeline::set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever) {
    hnsw_retriever_ = retriever;
}

std::vector<rag::RetrievalResult> NaivePipeline::retrieve_vectors(
    const std::string& query, int top_k) {
    if (!hnsw_retriever_) {
        RAG_ERROR("HNSW 检索器未设置");
        return {};
    }

    try {
        return hnsw_retriever_->retrieve(query, top_k);
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("向量检索异常: ") + e.what());
        return {};
    }
}

}  // namespace rag::modular
