/**
 * @file hybrid_pipeline.cpp
 * @brief HybridPipeline 实现
 *
 * 流程: Query → Vector + BM25 + Graph → RRF融合 → LLM
 */

#include "rag/modular/pipeline/hybrid_pipeline.h"
#include "rag/logger.h"
#include <chrono>

namespace rag::modular {

HybridPipeline::HybridPipeline()
    : initialized_(false),
      adaptive_rrf_(std::make_unique<rag::AdaptiveRRF>()) {}

HybridPipeline::~HybridPipeline() = default;

bool HybridPipeline::init(const ModularConfig& config) {
    RAG_INFO("初始化 HybridPipeline...");

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

    // 初始化自适应 RRF 融合器
    adaptive_rrf_->set_rrf_k(config.retrieval.rrf_k);

    initialized_ = true;
    RAG_INFO("HybridPipeline 初始化完成");
    return true;
}

bool HybridPipeline::is_ready() const {
    return initialized_ && llm_ && llm_->is_loaded()
           && hnsw_retriever_ && bm25_retriever_ && graph_retriever_;
}

ModularQueryResult HybridPipeline::query(const ModularQuery& query) {
    ModularQueryResult result;
    auto start_time = std::chrono::steady_clock::now();

    // 检查是否就绪
    if (!is_ready()) {
        result.error_message = "Pipeline 未就绪，请先调用 init() 或设置必要的检索器";
        RAG_ERROR(result.error_message);
        return result;
    }

    int top_k = query.top_k > 0 ? query.top_k : config_.retrieval.top_k;

    // Step 1: 并行执行三路检索
    auto retrieval_start = std::chrono::steady_clock::now();

    // 向量检索 (HNSW)
    auto hnsw_results = retrieve_vectors(query.text, top_k);

    // BM25 检索
    auto bm25_results = retrieve_bm25(query.text, top_k);

    // Graph 检索
    auto graph_results = retrieve_graph(query.text, top_k);

    result.retrieval_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - retrieval_start).count();

    RAG_INFO("三路检索完成 - HNSW: " + std::to_string(hnsw_results.size()) +
                ", BM25: " + std::to_string(bm25_results.size()) +
                ", Graph: " + std::to_string(graph_results.size()));

    if (hnsw_results.empty() && bm25_results.empty() && graph_results.empty()) {
        RAG_WARN("所有检索结果为空");
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    // Step 2: 三路 RRF 融合
    auto fused_results = fuse_with_rrf(hnsw_results, bm25_results, graph_results, top_k);

    if (fused_results.empty()) {
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    // 保存检索结果
    result.context = fused_results;

    // Step 3: 构建上下文字符串
    std::string context_str = build_context(query.text, fused_results);

    // Step 4: 构建提示词并生成回答
    std::string prompt = "请根据以下上下文信息回答问题。如果上下文中没有相关信息，请说明无法回答。\n\n"
                       "问题: " + query.text + "\n\n"
                       "上下文:\n" + context_str + "\n\n"
                       "回答:";

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

    RAG_INFO("HybridPipeline 查询完成，检索: " + std::to_string(result.retrieval_time_ms) +
                "ms, 生成: " + std::to_string(result.generation_time_ms) + "ms");

    return result;
}

void HybridPipeline::set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever) {
    hnsw_retriever_ = retriever;
}

void HybridPipeline::set_bm25_retriever(std::shared_ptr<rag::BM25Retriever> retriever) {
    bm25_retriever_ = retriever;
}

void HybridPipeline::set_graph_retriever(std::shared_ptr<rag::GraphRetriever> retriever) {
    graph_retriever_ = retriever;
}

std::vector<rag::RetrievalResult> HybridPipeline::retrieve_vectors(
    const std::string& query, int top_k) {
    if (!hnsw_retriever_) {
        return {};
    }

    try {
        return hnsw_retriever_->retrieve(query, top_k);
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("向量检索异常: ") + e.what());
        return {};
    }
}

std::vector<rag::RetrievalResult> HybridPipeline::retrieve_bm25(
    const std::string& query, int top_k) {
    if (!bm25_retriever_) {
        return {};
    }

    try {
        return bm25_retriever_->retrieve(query, top_k);
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("BM25 检索异常: ") + e.what());
        return {};
    }
}

std::vector<rag::RetrievalResult> HybridPipeline::retrieve_graph(
    const std::string& query, int top_k) {
    if (!graph_retriever_) {
        return {};
    }

    try {
        rag::RetrievalConfig config;
        config.top_k = top_k;
        auto graph_result = graph_retriever_->retrieve(query, config);

        // 将 Graph 检索结果转换为 RetrievalResult 格式
        std::vector<rag::RetrievalResult> results;
        for (const auto& chunk : graph_result.chunks) {
            rag::RetrievalResult r;
            r.chunk = chunk.chunk;
            r.score = chunk.score;
            r.source = "graph:" + chunk.source;
            results.push_back(r);
        }

        return results;
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Graph 检索异常: ") + e.what());
        return {};
    }
}

std::vector<rag::RetrievalResult> HybridPipeline::fuse_with_rrf(
    const std::vector<rag::RetrievalResult>& hnsw_results,
    const std::vector<rag::RetrievalResult>& bm25_results,
    const std::vector<rag::RetrievalResult>& graph_results,
    int top_k) {
    if (hnsw_results.empty() && bm25_results.empty() && graph_results.empty()) {
        return {};
    }

    try {
        return adaptive_rrf_->fuse_with_graph(hnsw_results, bm25_results, graph_results,
                                              rag::QueryType::FACTUAL, top_k);
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("三路 RRF 融合异常: ") + e.what());
        // 降级: 尝试两路融合
        if (!hnsw_results.empty() || !bm25_results.empty()) {
            return adaptive_rrf_->fuse(hnsw_results, bm25_results,
                                       rag::QueryType::FACTUAL, top_k);
        }
        return hnsw_results.empty() ? bm25_results : hnsw_results;
    }
}

}  // namespace rag::modular
