/**
 * @file graph_pipeline.cpp
 * @brief GraphPipeline 实现
 *
 * 流程: Query → 实体提取 → 图检索 → 子图 → Context → LLM
 */

#include "rag/modular/pipeline/graph_pipeline.h"
#include "rag/logger.h"
#include <chrono>
#include <algorithm>

namespace rag::modular {

GraphPipeline::GraphPipeline()
    : initialized_(false),
      hybrid_mode_(false) {
    // 设置默认的 Graph 检索配置
    graph_config_.max_hops = 2;
    graph_config_.max_entities = 50;
    graph_config_.max_relations = 100;
    graph_config_.entity_similarity_threshold = 0.6f;
    graph_config_.include_subgraph = true;
    graph_config_.include_text_chunks = true;
}

GraphPipeline::~GraphPipeline() = default;

bool GraphPipeline::init(const ModularConfig& config) {
    RAG_INFO("初始化 GraphPipeline...");

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
    RAG_INFO("GraphPipeline 初始化完成");
    return true;
}

bool GraphPipeline::is_ready() const {
    return initialized_ && llm_ && llm_->is_loaded()
           && graph_retriever_ && entity_extractor_;
}

ModularQueryResult GraphPipeline::query(const ModularQuery& query) {
    ModularQueryResult result;
    auto start_time = std::chrono::steady_clock::now();

    // 检查是否就绪
    if (!is_ready()) {
        result.error_message = "Pipeline 未就绪，请先调用 init() 或设置必要的组件";
        RAG_ERROR(result.error_message);
        return result;
    }

    int top_k = query.top_k > 0 ? query.top_k : config_.retrieval.top_k;

    // Step 1: 从查询中提取实体
    auto extraction_start = std::chrono::steady_clock::now();
    auto entities = extract_entities(query.text);
    int64_t extraction_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - extraction_start).count();

    RAG_INFO("实体提取完成，提取到 " + std::to_string(entities.size()) +
                " 个实体，耗时: " + std::to_string(extraction_time_ms) + "ms");

    // Step 2: 执行图检索
    auto retrieval_start = std::chrono::steady_clock::now();
    rag::RetrievalConfig retrieval_config;
    retrieval_config.top_k = top_k;

    rag::GraphRetrievalResult graph_result;
    std::vector<rag::RetrievalResult> vector_results;

    if (!entities.empty()) {
        // 使用提取的实体进行图检索
        graph_result = retrieve_graph(entities, retrieval_config);
    }

    // 如果启用了混合模式，同时执行向量检索
    if (hybrid_mode_ && hnsw_retriever_) {
        vector_results = retrieve_vectors(query.text, top_k);
    }

    int64_t retrieval_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - retrieval_start).count();

    // Step 3: 检查是否有检索结果
    if (graph_result.chunks.empty() && vector_results.empty()) {
        RAG_WARN("图检索和向量检索结果都为空");
        result.success = true;
        result.answer = "抱歉，未找到与您查询相关的文档内容。";
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return result;
    }

    // Step 4: 构建上下文字符串
    std::string context_str = build_graph_context(graph_result.subgraph, graph_result.chunks);

    // 如果有向量检索结果，追加到上下文
    if (!vector_results.empty()) {
        context_str += "\n\n【向量检索结果】\n";
        context_str += build_context(query.text, vector_results);
        result.context = vector_results;
    }

    // 保存图检索的 chunks 到 context
    for (const auto& chunk : graph_result.chunks) {
        rag::RetrievalResult r;
        r.chunk = chunk.chunk;
        r.score = chunk.score;
        r.source = "graph:" + chunk.source;
        result.context.push_back(r);
    }

    // Step 5: 构建提示词并生成回答
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

    result.retrieval_time_ms = extraction_time_ms + retrieval_time_ms;
    result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    result.success = true;

    RAG_INFO("GraphPipeline 查询完成，实体提取: " + std::to_string(extraction_time_ms) +
                "ms, 检索: " + std::to_string(retrieval_time_ms) +
                "ms, 生成: " + std::to_string(result.generation_time_ms) + "ms");

    return result;
}

void GraphPipeline::set_entity_extractor(std::shared_ptr<rag::EntityExtractor> extractor) {
    entity_extractor_ = extractor;
}

void GraphPipeline::set_graph_retriever(std::shared_ptr<rag::GraphRetriever> retriever) {
    graph_retriever_ = retriever;
}

void GraphPipeline::set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever) {
    hnsw_retriever_ = retriever;
}

std::vector<rag::KGEntity> GraphPipeline::extract_entities(const std::string& query) {
    if (!entity_extractor_) {
        RAG_WARN("实体提取器未设置");
        return {};
    }

    try {
        auto extraction_result = entity_extractor_->extract(query, "query_" + query);
        RAG_DEBUG("提取到 " + std::to_string(extraction_result.entities.size()) +
                     " 个实体");
        return extraction_result.entities;
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("实体提取异常: ") + e.what());
        return {};
    }
}

rag::GraphRetrievalResult GraphPipeline::retrieve_graph(
    const std::vector<rag::KGEntity>& entities,
    const rag::RetrievalConfig& config) {
    if (!graph_retriever_) {
        RAG_ERROR("Graph 检索器未设置");
        return {};
    }

    if (entities.empty()) {
        return {};
    }

    try {
        // 构建查询字符串（使用实体名称）
        std::ostringstream oss;
        for (const auto& entity : entities) {
            if (oss.str().empty()) {
                oss << entity.name;
            } else {
                oss << " " << entity.name;
            }
        }

        auto result = graph_retriever_->retrieve(oss.str(), config);
        RAG_DEBUG("Graph 检索到 " + std::to_string(result.chunks.size()) +
                     " 个 chunks");
        return result;
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Graph 检索异常: ") + e.what());
        return {};
    }
}

std::vector<rag::RetrievalResult> GraphPipeline::retrieve_vectors(
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

std::string GraphPipeline::build_graph_context(
    const rag::KGSubgraph& subgraph,
    const std::vector<rag::RetrievedChunk>& chunks) {
    std::ostringstream oss;

    if (subgraph.entities.empty() && subgraph.relations.empty() && chunks.empty()) {
        return "";
    }

    oss << "【知识图谱上下文】\n\n";

    // 添加实体信息
    if (!subgraph.entities.empty()) {
        oss << "【相关实体】\n";
        for (const auto& entity : subgraph.entities) {
            oss << "- " << entity.name;
            if (entity.type != rag::EntityType::UNKNOWN) {
                oss << " (type:" << static_cast<int>(entity.type) << ")";
            }
            if (!entity.description.empty()) {
                oss << ": " << entity.description;
            }
            oss << "\n";
        }
        oss << "\n";
    }

    // 添加关系信息
    if (!subgraph.relations.empty()) {
        oss << "【相关关系】\n";
        for (const auto& relation : subgraph.relations) {
            oss << "- " << relation.source_id << " --[" << relation.type << "]--> "
                << relation.target_id;
            if (!relation.description.empty()) {
                oss << ": " << relation.description;
            }
            oss << "\n";
        }
        oss << "\n";
    }

    // 添加关联的文本块
    if (!chunks.empty()) {
        oss << "【关联文档片段】\n";
        for (size_t i = 0; i < chunks.size(); ++i) {
            const auto& chunk = chunks[i];
            oss << "【来源 " << (i + 1) << "】(得分: " << chunk.score
                << ", 来源: " << chunk.source << ")\n";
            if (!chunk.chunk.metadata.file_path.empty()) {
                oss << "文件: " << chunk.chunk.metadata.file_path;
                if (chunk.chunk.chunk_index > 0) {
                    oss << " (块 " << chunk.chunk.chunk_index << ")";
                }
                oss << "\n";
            }
            oss << chunk.chunk.content << "\n\n";
        }
    }

    return oss.str();
}

bool GraphPipeline::is_graph_suitable_query(const std::string& query) {
    // 简单的启发式判断：包含实体名称、关系词汇等适合图检索
    // 实际应用中可以使用分类器来判断

    std::vector<std::string> entity_indicators = {
        "谁", "什么人", "哪个公司", "哪个组织",
        "什么时候", "何时", "什么时间",
        "在哪", "哪里", "哪个地点",
        "什么关系", "有什么关系", "如何相关"
    };

    std::vector<std::string> relation_indicators = {
        "和", "与", "关联", "关系", "连接",
        "导致", "引起", "产生", "为了", "目的"
    };

    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    for (const auto& indicator : entity_indicators) {
        if (lower_query.find(indicator) != std::string::npos) {
            return true;
        }
    }

    for (const auto& indicator : relation_indicators) {
        if (lower_query.find(indicator) != std::string::npos) {
            return true;
        }
    }

    return false;
}

}  // namespace rag::modular
