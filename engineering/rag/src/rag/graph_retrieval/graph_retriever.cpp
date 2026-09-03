                                                                    /**
 * @file graph_retriever.cpp
 * @brief Graph 检索器实现
 */

#include "rag/graph_retriever.h"
#include "rag/retriever.h"
#include "rag/knowledge_graph.h"
#include "rag/logger.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <unordered_set>

namespace rag {

// ========== BaseGraphRetriever 实现 ==========

BaseGraphRetriever::BaseGraphRetriever(const GraphRetrievalConfig& config)
    : config_(config) {}

std::vector<std::string> BaseGraphRetriever::extract_candidate_entities(
    const std::string& query) {

    std::vector<std::string> candidates;

    // 简单分词 - 按空格和标点分割
    std::string current;
    for (char c : query) {
        if (std::isspace(c) || std::ispunct(c)) {
            if (!current.empty() && current.length() >= 2) {
                candidates.push_back(current);
                current.clear();
            }
        } else {
            current += static_cast<char>(std::tolower(c));
        }
    }
    if (!current.empty() && current.length() >= 2) {
        candidates.push_back(current);
    }

    // 尝试提取中文词组 (2-4个连续中文字符)
    std::string chinese;
    for (char c : query) {
        if (c >= 0x80) {  // 中文字符范围
            chinese += c;
        } else {
            if (chinese.length() >= 4) {
                candidates.push_back(chinese);
            }
            chinese.clear();
        }
    }
    if (chinese.length() >= 4) {
        candidates.push_back(chinese);
    }

    return candidates;
}

std::vector<KGEntity> BaseGraphRetriever::search_similar_entities(
    const std::string& term) {

    std::vector<KGEntity> results;

    if (kg_ == nullptr) {
        return results;
    }

    // 1. 精确匹配名称
    auto exact = kg_->get_entity_by_name(term);
    if (exact.has_value()) {
        results.push_back(exact.value());
    }

    // 2. 搜索名称包含该词的实体
    auto all_entities = kg_->get_all_entities();
    std::string lower_term = term;
    std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(), ::tolower);

    for (const auto& entity : all_entities) {
        std::string lower_name = entity.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

        if (lower_name.find(lower_term) != std::string::npos) {
            if (std::find(results.begin(), results.end(), entity) == results.end()) {
                results.push_back(entity);
            }
        }
    }

    // 3. 搜索别名匹配
    for (const auto& entity : all_entities) {
        for (const auto& alias : entity.aliases) {
            std::string lower_alias = alias;
            std::transform(lower_alias.begin(), lower_alias.end(), lower_alias.begin(), ::tolower);
            if (lower_alias.find(lower_term) != std::string::npos) {
                if (std::find(results.begin(), results.end(), entity) == results.end()) {
                    results.push_back(entity);
                    break;
                }
            }
        }
    }

    return results;
}

std::vector<EntityLinkResult> BaseGraphRetriever::link_query_entities(
    const std::string& query) {

    std::vector<EntityLinkResult> results;

    // 1. 提取候选实体
    auto candidates = extract_candidate_entities(query);

    // 2. 对每个候选进行链接
    for (const auto& term : candidates) {
        auto entities = search_similar_entities(term);

        if (!entities.empty()) {
            // 取最匹配的实体
            const auto& best = entities[0];

            EntityLinkResult link_result;
            link_result.query_term = term;
            link_result.linked_entity_id = best.id;
            link_result.linked_entity_name = best.name;

            // 计算相似度 (简化版：完全匹配为1.0，否则基于名称长度)
            if (best.name == term) {
                link_result.similarity = 1.0f;
                link_result.is_exact_match = true;
            } else {
                link_result.similarity = 0.7f;
                link_result.is_exact_match = false;
            }

            results.push_back(link_result);
        }
    }

    RAG_DEBUG("Linked " + std::to_string(results.size()) + " entities from query");

    return results;
}

KGSubgraph BaseGraphRetriever::expand_subgraph(
    const std::string& entity_id,
    int max_hops) {

    KGSubgraph subgraph;

    if (kg_ == nullptr) {
        return subgraph;
    }

    subgraph.root_entity_id = entity_id;
    subgraph.hop_count = 0;

    // 获取根实体
    auto root = kg_->get_entity(entity_id);
    if (root.has_value()) {
        subgraph.entities.push_back(root.value());

        // BFS 扩展
        std::unordered_set<std::string> visited;
        visited.insert(entity_id);

        std::vector<std::string> current_level = {entity_id};
        int current_hops = 0;

        while (current_hops < max_hops && !current_level.empty()) {
            std::vector<std::string> next_level;

            for (const auto& curr_id : current_level) {
                // 获取邻居实体
                auto neighbors = kg_->get_neighbors(curr_id);

                for (const auto& neighbor : neighbors) {
                    if (visited.find(neighbor.id) == visited.end()) {
                        visited.insert(neighbor.id);
                        subgraph.entities.push_back(neighbor);
                        next_level.push_back(neighbor.id);
                    }
                }

                // 获取关联关系
                auto relations = kg_->get_relations(curr_id);
                for (const auto& rel : relations) {
                    if (rel.source_id == curr_id || rel.target_id == curr_id) {
                        subgraph.relations.push_back(rel);
                    }
                }
            }

            current_level = next_level;
            current_hops++;
        }

        subgraph.hop_count = current_hops;
    }

    return subgraph;
}

// ========== build_result 实现 ==========

GraphRetrievalResult BaseGraphRetriever::build_result(
    const std::string& query,
    const std::vector<EntityLinkResult>& linked_entities,
    const KGSubgraph& subgraph) {

    GraphRetrievalResult result;
    result.query = query;
    result.subgraph = subgraph;

    // 填充匹配的实体
    for (const auto& link : linked_entities) {
        auto entity = kg_->get_entity(link.linked_entity_id);
        if (entity.has_value()) {
            result.matched_entities.push_back(entity.value());
        }
    }

    return result;
}

GraphRetrievalResult BaseGraphRetriever::retrieve(
    const std::string& query,
    const RetrievalConfig& config) {

    GraphRetrievalResult result;
    result.query = query;

    auto start = std::chrono::steady_clock::now();

    // 1. 实体链接
    auto linked_entities = link_query_entities(query);
    result.matched_entities.reserve(linked_entities.size());

    for (const auto& link : linked_entities) {
        auto entity = kg_->get_entity(link.linked_entity_id);
        if (entity.has_value()) {
            result.matched_entities.push_back(entity.value());
        }
    }

    // 2. 子图扩展
    if (config_.include_subgraph && !result.matched_entities.empty()) {
        // 合并所有匹配实体的子图
        std::unordered_set<std::string> seen_entities;
        std::unordered_set<std::string> seen_relations;

        for (const auto& entity : result.matched_entities) {
            auto subgraph = expand_subgraph(entity.id, config_.max_hops);

            for (const auto& e : subgraph.entities) {
                if (seen_entities.find(e.id) == seen_entities.end()) {
                    seen_entities.insert(e.id);
                    result.subgraph.entities.push_back(e);
                }
            }

            for (const auto& r : subgraph.relations) {
                if (seen_relations.find(r.id) == seen_relations.end()) {
                    seen_relations.insert(r.id);
                    result.subgraph.relations.push_back(r);
                }
            }
        }

        result.subgraph.hop_count = config_.max_hops;
    }

    // 3. 获取关联文本块 (如果有 text_retriever_)
    if (config_.include_text_chunks && text_retriever_ != nullptr) {
        // 基于匹配的实体构建文本查询
        std::string entity_query;
        for (const auto& entity : result.matched_entities) {
            if (!entity_query.empty()) entity_query += " ";
            entity_query += entity.name;
            for (const auto& alias : entity.aliases) {
                entity_query += " " + alias;
            }
        }

        if (!entity_query.empty()) {
            auto text_result = text_retriever_->retrieve(entity_query, config.top_k);
            // 转换 RetrievalResult 到 RetrievedChunk
            for (const auto& r : text_result) {
                result.chunks.push_back({
                    r.chunk,
                    r.score,
                    name()
                });
            }
        }
    }

    // 4. 计算分数
    result.score = static_cast<float>(result.matched_entities.size()) * 0.5f +
                   static_cast<float>(result.subgraph.relations.size()) * 0.3f +
                   static_cast<float>(result.chunks.size()) * 0.2f;

    result.method = name();

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    RAG_DEBUG("Graph retrieval: " + std::to_string(result.matched_entities.size())
              + " entities, " + std::to_string(result.subgraph.relations.size())
              + " relations, " + std::to_string(result.chunks.size()) + " chunks");

    return result;
}

// ========== HybridGraphRetriever 实现 ==========

HybridGraphRetriever::HybridGraphRetriever(const GraphRetrievalConfig& config)
    : BaseGraphRetriever(config) {}

std::vector<RetrievedChunk> HybridGraphRetriever::rrf_fusion(
    const std::vector<RetrievalResult>& graph_results,
    const std::vector<RetrievalResult>& vector_results) {

    std::unordered_map<std::string, std::pair<float, RetrievalResult>> score_map;

    // Graph 检索结果评分
    for (size_t i = 0; i < graph_results.size(); ++i) {
        float rrf_score = 1.0f / (rrf_k_ + i + 1);
        score_map[graph_results[i].chunk.id] = {
            rrf_score * 0.6f,  // Graph 权重
            graph_results[i]
        };
    }

    // Vector 检索结果评分
    for (size_t i = 0; i < vector_results.size(); ++i) {
        float rrf_score = 1.0f / (rrf_k_ + i + 1);
        auto it = score_map.find(vector_results[i].chunk.id);
        if (it != score_map.end()) {
            it->second.first += rrf_score * 0.4f;  // Vector 权重
        } else {
            score_map[vector_results[i].chunk.id] = {
                rrf_score * 0.4f,
                vector_results[i]
            };
        }
    }

    // 转换为向量并排序
    std::vector<std::pair<std::string, std::pair<float, RetrievalResult>>> scores(
        score_map.begin(), score_map.end());
    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) { return a.second.first > b.second.first; });

    // 构建结果
    std::vector<RetrievedChunk> result;
    for (const auto& entry : scores) {
        RetrievedChunk rc;
        rc.chunk = entry.second.second.chunk;
        rc.score = entry.second.first;
        rc.source = name();
        result.push_back(rc);
    }

    return result;
}

GraphRetrievalResult HybridGraphRetriever::retrieve(
    const std::string& query,
    const RetrievalConfig& config) {

    GraphRetrievalResult result;
    result.query = query;

    auto start = std::chrono::steady_clock::now();

    // 1. Graph 检索
    auto graph_result = BaseGraphRetriever::retrieve(query, config);
    result.subgraph = graph_result.subgraph;
    result.matched_entities = graph_result.matched_entities;

    // 2. Vector 检索 (如果有 text_retriever_)
    std::vector<RetrievalResult> vector_results;
    if (text_retriever_ != nullptr) {
        vector_results = text_retriever_->retrieve(query, config.top_k);
    }

    // 3. 转换 graph_result.chunks 为 RetrievalResult
    std::vector<RetrievalResult> graph_results;
    for (const auto& rc : graph_result.chunks) {
        RetrievalResult rr;
        rr.chunk = rc.chunk;
        rr.score = rc.score;
        rr.source = rc.source;
        graph_results.push_back(rr);
    }

    // 4. RRF 融合
    result.chunks = rrf_fusion(graph_results, vector_results);

    // 5. 计算综合分数
    result.score = static_cast<float>(result.matched_entities.size()) * 0.4f +
                   static_cast<float>(result.subgraph.relations.size()) * 0.3f +
                   (!vector_results.empty() ?
                    static_cast<float>(result.chunks.size()) / static_cast<float>(vector_results.size()) * 0.3f : 0.0f);

    result.method = name();

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    RAG_DEBUG("Hybrid retrieval: " + std::to_string(result.matched_entities.size())
              + " entities, " + std::to_string(result.chunks.size()) + " fused chunks");

    return result;
}

// ========== 工厂函数 ==========

std::unique_ptr<GraphRetriever> create_graph_retriever(
    const std::string& type,
    const GraphRetrievalConfig& config) {

    if (type == "hybrid" || type == "hybrid-graph") {
        return std::make_unique<HybridGraphRetriever>(config);
    }

    return std::make_unique<BaseGraphRetriever>(config);
}

}  // namespace rag
