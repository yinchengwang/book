/**
 * @file advanced_pipeline.cpp
 * @brief AdvancedPipeline 实现
 *
 * 流程: Query → QueryExp → Vector+BM25 → RRF → Rerank → Context → LLM
 */

#include "rag/modular/pipeline/advanced_pipeline.h"
#include "rag/logger.h"
#include <chrono>

namespace rag::modular {

AdvancedPipeline::AdvancedPipeline()
    : initialized_(false),
      adaptive_rrf_(std::make_unique<rag::AdaptiveRRF>()) {}

AdvancedPipeline::~AdvancedPipeline() = default;

bool AdvancedPipeline::init(const ModularConfig& config) {
    RAG_LOG_INFO("初始化 AdvancedPipeline...");

    // 保存配置
    config_ = config;

    // 初始化 LLM 服务
    if (!config.llm.model_path.empty()) {
        llm_ = rag::create_llm_service();
        if (llm_) {
            llm_->load(config.llm.model_path, config.llm);
            RAG_LOG_INFO("LLM 模型加载完成: " + config.llm.model_path);
        } else {
            RAG_LOG_WARN("LLM 服务创建失败");
        }
    }

    // 初始化查询扩展器 (如果需要)
    // 注意: QueryExpander 需要外部设置或在此创建

    // 初始化自适应 RRF 融合器
    adaptive_rrf_->set_rrf_k(config.retrieval.rrf_k);

    initialized_ = true;
    RAG_LOG_INFO("AdvancedPipeline 初始化完成");
    return true;
}

bool AdvancedPipeline::is_ready() const {
    return initialized_ && llm_ && llm_->is_loaded()
           && hnsw_retriever_ && bm25_retriever_;
}

ModularQueryResult AdvancedPipeline::query(const ModularQuery& query) {
    ModularQueryResult result;
    auto start_time = std::chrono::steady_clock::now();

    // 检查是否就绪
    if (!is_ready()) {
        result.error_message = "Pipeline 未就绪，请先调用 init() 或设置必要的检索器";
        RAG_LOG_ERROR(result.error_message);
        return result;
    }

    int top_k = query.top_k > 0 ? query.top_k : config_.retrieval.top_k;

    // Step 1: 查询扩展
    std::vector<std::string> expanded_queries = {query.text};
    if (query_expander_) {
        auto expansion_start = std::chrono::steady_clock::now();
        expanded_queries = expand_query(query.text);
        RAG_LOG_INFO("查询扩展完成，生成 " + std::to_string(expanded_queries.size()) +
                    " 个查询变体");
        (void)expansion_start;  // 忽略未使用警告
    }

    // Step 2: 并行执行向量检索和 BM25 检索
    auto retrieval_start = std::chrono::steady_clock::now();

    std::vector<rag::RetrievalResult> all_hnsw_results;
    std::vector<rag::RetrievalResult> all_bm25_results;

    // 对每个扩展查询执行检索
    for (const auto& q : expanded_queries) {
        auto hnsw_results = retrieve_vectors(q, top_k);
        auto bm25_results = retrieve_bm25(q, top_k);

        all_hnsw_results.insert(all_hnsw_results.end(), hnsw_results.begin(), hnsw_results.end());
        all_bm25_results.insert(all_bm25_results.end(), bm25_results.begin(), bm25_results.end());
    }

    result.retrieval_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - retrieval_start).count();

    if (all_hnsw_results.empty() && all_bm25_results.empty()) {
        RAG_LOG_WARN("所有检索结果为空");
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    // Step 3: RRF 融合
    auto fused_results = fuse_with_rrf(all_hnsw_results, all_bm25_results, top_k);

    if (fused_results.empty()) {
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    // Step 4: 重排序 (如果有重排序器)
    auto reranked_results = fused_results;
    if (reranker_) {
        reranked_results = rerank_results(query.text, fused_results, top_k);
    }

    // 保存检索结果
    result.context = reranked_results;

    // Step 5: 构建上下文字符串
    std::string context_str = build_context(query.text, reranked_results);

    // Step 6: 构建提示词并生成回答
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

    RAG_LOG_INFO("AdvancedPipeline 查询完成，检索: " + std::to_string(result.retrieval_time_ms) +
                "ms, 生成: " + std::to_string(result.generation_time_ms) + "ms");

    return result;
}

void AdvancedPipeline::set_query_expander(std::shared_ptr<rag::QueryExpander> expander) {
    query_expander_ = expander;
}

void AdvancedPipeline::set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever) {
    hnsw_retriever_ = retriever;
}

void AdvancedPipeline::set_bm25_retriever(std::shared_ptr<rag::BM25Retriever> retriever) {
    bm25_retriever_ = retriever;
}

void AdvancedPipeline::set_reranker(std::shared_ptr<rag::Reranker> reranker) {
    reranker_ = reranker;
}

std::vector<std::string> AdvancedPipeline::expand_query(const std::string& query) {
    if (!query_expander_) {
        return {query};
    }

    try {
        auto expansion = query_expander_->expand(query);
        if (expansion.expanded_queries.empty()) {
            return {query};
        }
        return expansion.expanded_queries;
    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("查询扩展异常: ") + e.what());
        return {query};
    }
}

std::vector<rag::RetrievalResult> AdvancedPipeline::retrieve_vectors(
    const std::string& query, int top_k) {
    if (!hnsw_retriever_) {
        return {};
    }

    try {
        return hnsw_retriever_->retrieve(query, top_k);
    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("向量检索异常: ") + e.what());
        return {};
    }
}

std::vector<rag::RetrievalResult> AdvancedPipeline::retrieve_bm25(
    const std::string& query, int top_k) {
    if (!bm25_retriever_) {
        return {};
    }

    try {
        return bm25_retriever_->retrieve(query, top_k);
    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("BM25 检索异常: ") + e.what());
        return {};
    }
}

std::vector<rag::RetrievalResult> AdvancedPipeline::fuse_with_rrf(
    const std::vector<rag::RetrievalResult>& hnsw_results,
    const std::vector<rag::RetrievalResult>& bm25_results,
    int top_k) {
    if (hnsw_results.empty() && bm25_results.empty()) {
        return {};
    }

    try {
        return adaptive_rrf_->fuse(hnsw_results, bm25_results,
                                  rag::QueryType::SIMPLE, top_k);
    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("RRF 融合异常: ") + e.what());
        // 降级: 返回 HNSW 结果
        if (!hnsw_results.empty()) {
            return hnsw_results;
        }
        return bm25_results;
    }
}

std::vector<rag::RetrievalResult> AdvancedPipeline::rerank_results(
    const std::string& query,
    const std::vector<rag::RetrievalResult>& candidates,
    int top_n) {
    if (!reranker_ || candidates.empty()) {
        return candidates;
    }

    try {
        // 转换为 Chunk 列表
        std::vector<rag::Chunk> chunks;
        for (const auto& result : candidates) {
            chunks.push_back(result.chunk);
        }

        // 执行重排序
        auto reranked = reranker_->rerank(query, chunks, top_n);

        // 构建重排后的结果
        std::vector<rag::RetrievalResult> results;
        for (size_t i = 0; i < reranked.size() && i < candidates.size(); ++i) {
            rag::RetrievalResult r;
            r.chunk = reranked[i].chunk;
            r.score = reranked[i].score;
            r.source = "reranked";
            r.rank = static_cast<int>(i);
            results.push_back(r);
        }

        return results;
    } catch (const std::exception& e) {
        RAG_LOG_ERROR(std::string("重排序异常: ") + e.what());
        return candidates;
    }
}

}  // namespace rag::modular
