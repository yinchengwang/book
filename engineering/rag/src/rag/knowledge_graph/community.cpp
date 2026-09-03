/**
 * @file community.cpp
 * @brief 知识图谱社区检测与摘要实现
 */

#include "rag/community.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <queue>
#include <random>
#include <regex>
#include <sstream>

namespace rag {

// ========== CommunityDetector ==========

CommunityDetector::CommunityDetector(const Config& config) : config_(config) {}

CommunityDetector::~CommunityDetector() = default;

CommunityDetectionResult CommunityDetector::detect_communities(
    const KnowledgeGraph& graph,
    const std::string& algorithm) {

    auto start = std::chrono::steady_clock::now();

    CommunityDetectionResult result;
    result.algorithm = algorithm;

    if (graph.entities.empty()) {
        return result;
    }

    // 构建邻接表
    build_adjacency_list(graph);

    // 计算中心性
    compute_centralities(graph);

    // 执行社区检测
    if (algorithm == "louvain") {
        result = detect_louvain(graph);
    } else if (algorithm == "leiden") {
        result = detect_leiden(graph);
    } else {
        result = detect_label_propagation(graph);
    }

    // 合并小社区
    result.communities = merge_small_communities(result.communities);

    // 提取中心实体
    for (auto& community : result.communities) {
        community.central_entities = extract_central_entities(community, 5);
        community.size = static_cast<int>(community.entity_ids.size());
    }

    // 更新缓存
    if (config_.cache_results) {
        entity_community_cache_ = result.entity_to_community;
        cached_communities_ = result.communities;
    }

    // 计算模块度
    result.modularity = calculate_modularity(graph);

    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    return result;
}

CommunityDetectionResult CommunityDetector::detect_hierarchical_communities(
    const KnowledgeGraph& graph,
    int levels) {

    auto start = std::chrono::steady_clock::now();

    CommunityDetectionResult result;
    result.algorithm = "hierarchical";

    if (levels <= 0) {
        return result;
    }

    // 第一层: 基础社区检测
    auto level0_result = detect_louvain(graph);
    result.communities = level0_result.communities;
    result.entity_to_community = level0_result.entity_to_community;

    // 高层: 合并社区
    for (int level = 1; level < levels; ++level) {
        // 创建高层社区
        std::unordered_map<std::string, Community> merged;

        for (const auto& community : result.communities) {
            std::string parent_id = community.parent_id.empty() ?
                community.id : community.parent_id;

            if (merged.find(parent_id) == merged.end()) {
                merged[parent_id] = Community();
                merged[parent_id].id = "level_" + std::to_string(level) + "_" + parent_id;
                merged[parent_id].parent_id = community.id;
                merged[parent_id].level = level;
            }

            merged[parent_id].entity_ids.insert(
                merged[parent_id].entity_ids.end(),
                community.entity_ids.begin(),
                community.entity_ids.end()
            );
        }

        // 添加高层社区
        for (auto& [id, community] : merged) {
            if (community.entity_ids.size() >= config_.min_community_size) {
                result.communities.push_back(community);
            }
        }
    }

    result.total_levels = levels;
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    return result;
}

std::string CommunityDetector::get_entity_community(const std::string& entity_id) const {
    auto it = entity_community_cache_.find(entity_id);
    if (it != entity_community_cache_.end()) {
        return it->second;
    }
    return "";
}

std::vector<Entity> CommunityDetector::get_community_entities(
    const std::string& community_id) const {

    std::vector<Entity> entities;

    for (const auto& community : cached_communities_) {
        if (community.id == community_id) {
            for (const auto& entity_id : community.entity_ids) {
                // 从图中获取实体
                // 简化: 直接添加 ID
                Entity e;
                e.id = entity_id;
                entities.push_back(e);
            }
            break;
        }
    }

    return entities;
}

std::vector<Relation> CommunityDetector::get_community_relations(
    const std::string& community_id) const {

    // 获取社区内的实体
    auto entity_ids = std::unordered_set<std::string>();
    for (const auto& community : cached_communities_) {
        if (community.id == community_id) {
            entity_ids.insert(community.entity_ids.begin(), community.entity_ids.end());
            break;
        }
    }

    // 返回社区内的关系
    std::vector<Relation> relations;
    // 实际需要从图中获取
    return relations;
}

std::vector<std::string> CommunityDetector::get_sub_communities(
    const std::string& community_id) const {

    std::vector<std::string> sub_ids;
    for (const auto& community : cached_communities_) {
        if (community.parent_id == community_id) {
            sub_ids.push_back(community.id);
        }
    }
    return sub_ids;
}

double CommunityDetector::calculate_modularity(const KnowledgeGraph& graph) const {
    if (graph.entities.empty()) {
        return 0.0;
    }

    // 简化: 计算 Q = sum(e_ii - a_i^2)
    // 实际需要边和权重统计
    double m = graph.relations.size() * 2.0;  // 总边数
    double q = 0.0;

    for (const auto& community : cached_communities_) {
        // 社区内边数
        int internal = 0;
        for (const auto& e1 : community.entity_ids) {
            auto it = adjacency_list_.find(e1);
            if (it != adjacency_list_.end()) {
                for (const auto& e2 : it->second) {
                    if (entity_community_cache_.at(e2) == community.id) {
                        internal++;
                    }
                }
            }
        }

        // 节点度数和
        double a_i = 0.0;
        for (const auto& e : community.entity_ids) {
            auto it = adjacency_list_.find(e);
            if (it != adjacency_list_.end()) {
                a_i += it->second.size();
            }
        }

        if (m > 0) {
            q += (internal / m) - (a_i / m) * (a_i / m);
        }
    }

    return q;
}

void CommunityDetector::update_config(const Config& config) {
    config_ = config;
}

void CommunityDetector::build_adjacency_list(const KnowledgeGraph& graph) {
    adjacency_list_.clear();

    for (const auto& relation : graph.relations) {
        adjacency_list_[relation.source_id].push_back(relation.target_id);
        adjacency_list_[relation.target_id].push_back(relation.source_id);
    }
}

void CommunityDetector::compute_centralities(const KnowledgeGraph& graph) {
    betweenness_.clear();
    degree_centrality_.clear();

    // 度中心性
    for (const auto& [entity_id, neighbors] : adjacency_list_) {
        degree_centrality_[entity_id] = static_cast<float>(neighbors.size());
    }

    // 简化: 不计算介数中心性
    for (const auto& entity : graph.entities) {
        betweenness_[entity.id] = 0.0f;
    }
}

std::vector<Community> CommunityDetector::merge_small_communities(
    std::vector<Community>& communities) {

    std::vector<Community> merged;

    for (auto& community : communities) {
        if (community.entity_ids.size() >= config_.min_community_size &&
            community.entity_ids.size() <= config_.max_community_size) {
            merged.push_back(community);
        } else if (community.entity_ids.size() > config_.max_community_size) {
            // 拆分大社区 (简化: 均匀分配)
            int num_splits = (community.entity_ids.size() + config_.max_community_size - 1)
                             / config_.max_community_size;
            int split_size = community.entity_ids.size() / num_splits;

            for (int i = 0; i < num_splits; ++i) {
                Community sub;
                sub.id = community.id + "_split_" + std::to_string(i);
                sub.parent_id = community.id;
                sub.level = community.level;

                int start = i * split_size;
                int end = (i == num_splits - 1) ? community.entity_ids.size()
                                                : (i + 1) * split_size;

                sub.entity_ids.assign(
                    community.entity_ids.begin() + start,
                    community.entity_ids.begin() + end
                );

                merged.push_back(sub);
            }
        }
        // 小于 min_community_size 的社区忽略
    }

    return merged;
}

std::vector<std::string> CommunityDetector::extract_central_entities(
    const Community& community,
    int top_k) {

    // 按度中心性排序
    std::vector<std::pair<std::string, float>> scores;

    for (const auto& entity_id : community.entity_ids) {
        auto it = degree_centrality_.find(entity_id);
        float score = (it != degree_centrality_.end()) ? it->second : 0.0f;
        scores.push_back({entity_id, score});
    }

    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> central;
    for (int i = 0; i < std::min(top_k, static_cast<int>(scores.size())); ++i) {
        central.push_back(scores[i].first);
    }

    return central;
}

CommunityDetectionResult CommunityDetector::detect_louvain(const KnowledgeGraph& graph) {
    CommunityDetectionResult result;

    if (graph.entities.empty()) {
        return result;
    }

    // 初始化: 每个节点一个社区
    std::unordered_map<std::string, std::string> node_community;
    std::unordered_map<std::string, std::string> best_community;
    for (const auto& entity : graph.entities) {
        node_community[entity.id] = entity.id;
        best_community[entity.id] = entity.id;
    }

    // 迭代优化
    bool improved = true;
    int iterations = 0;

    while (improved && iterations < config_.max_iterations) {
        improved = false;
        iterations++;

        for (const auto& entity : graph.entities) {
            const std::string& current = node_community[entity.id];

            // 邻居社区
            std::unordered_map<std::string, int> neighbor_weights;
            auto it = adjacency_list_.find(entity.id);
            if (it != adjacency_list_.end()) {
                for (const auto& neighbor : it->second) {
                    const std::string& neighbor_comm = node_community[neighbor];
                    neighbor_weights[neighbor_comm]++;
                }
            }

            // 选择权重最大的邻居社区
            std::string best_neighbor = current;
            int best_weight = 0;

            for (const auto& [comm, weight] : neighbor_weights) {
                if (weight > best_weight) {
                    best_weight = weight;
                    best_neighbor = comm;
                }
            }

            if (best_neighbor != current) {
                node_community[entity.id] = best_neighbor;
                improved = true;
            }
        }
    }

    // 合并相同社区的节点
    std::unordered_map<std::string, Community> community_map;

    for (const auto& [entity_id, comm_id] : node_community) {
        if (community_map.find(comm_id) == community_map.end()) {
            Community c;
            c.id = comm_id;
            community_map[comm_id] = c;
        }
        community_map[comm_id].entity_ids.push_back(entity_id);
    }

    // 构建结果
    for (auto& [id, community] : community_map) {
        if (community.entity_ids.size() >= config_.min_community_size) {
            result.communities.push_back(community);
            for (const auto& entity_id : community.entity_ids) {
                result.entity_to_community[entity_id] = community.id;
            }
        }
    }

    result.success = true;
    return result;
}

CommunityDetectionResult CommunityDetector::detect_label_propagation(
    const KnowledgeGraph& graph) {

    CommunityDetectionResult result;

    if (graph.entities.empty()) {
        return result;
    }

    // 初始化标签为节点 ID
    std::unordered_map<std::string, std::string> labels;
    for (const auto& entity : graph.entities) {
        labels[entity.id] = entity.id;
    }

    // 异步标签传播
    std::random_device rd;
    std::mt19937 gen(rd());

    bool changed = true;
    int iterations = 0;

    while (changed && iterations < config_.max_iterations) {
        changed = false;
        iterations++;

        // 随机顺序遍历节点
        std::vector<std::string> node_ids;
        for (const auto& entity : graph.entities) {
            node_ids.push_back(entity.id);
        }
        std::shuffle(node_ids.begin(), node_ids.end(), gen);

        for (const auto& node_id : node_ids) {
            auto it = adjacency_list_.find(node_id);
            if (it == adjacency_list_.end() || it->second.empty()) {
                continue;
            }

            // 统计邻居标签频率
            std::unordered_map<std::string, int> label_counts;
            for (const auto& neighbor : it->second) {
                const std::string& label = labels[neighbor];
                label_counts[label]++;
            }

            // 选择最频繁的标签
            std::string best_label;
            int best_count = 0;
            for (const auto& [label, count] : label_counts) {
                if (count > best_count) {
                    best_count = count;
                    best_label = label;
                }
            }

            if (!best_label.empty() && best_label != labels[node_id]) {
                labels[node_id] = best_label;
                changed = true;
            }
        }
    }

    // 合并相同标签的节点
    std::unordered_map<std::string, Community> community_map;
    for (const auto& [entity_id, label] : labels) {
        if (community_map.find(label) == community_map.end()) {
            Community c;
            c.id = label;
            community_map[label] = c;
        }
        community_map[label].entity_ids.push_back(entity_id);
    }

    for (auto& [id, community] : community_map) {
        if (community.entity_ids.size() >= config_.min_community_size) {
            result.communities.push_back(community);
            for (const auto& entity_id : community.entity_ids) {
                result.entity_to_community[entity_id] = community.id;
            }
        }
    }

    result.success = true;
    return result;
}

CommunityDetectionResult CommunityDetector::detect_leiden(const KnowledgeGraph& graph) {
    // Leiden 是 Louvain 的改进，这里简化为调用 Louvain
    return detect_louvain(graph);
}

// ========== CommunitySummarizer ==========

CommunitySummarizer::CommunitySummarizer(std::shared_ptr<LLMService> llm_service)
    : llm_service_(llm_service) {}

CommunitySummaryResult CommunitySummarizer::generate_summary(
    const CommunitySummaryRequest& request) {

    if (!llm_service_) {
        CommunitySummaryResult result;
        result.community_id = request.community_id;
        result.summary = "LLM service not available";
        return result;
    }

    // 构建 prompt
    auto prompt = build_summary_prompt(
        request.community_id,
        *request.graph,
        request.context
    );

    try {
        auto response = llm_service_->generate(prompt);
        return parse_summary_response(response, request.community_id);
    } catch (const std::exception& e) {
        CommunitySummaryResult result;
        result.community_id = request.community_id;
        result.summary = "Error: " + std::string(e.what());
        return result;
    }
}

std::vector<CommunitySummaryResult> CommunitySummarizer::generate_summaries_batch(
    const std::vector<CommunitySummaryRequest>& requests,
    int max_parallel) {

    std::vector<CommunitySummaryResult> results;

    for (size_t i = 0; i < requests.size(); i += max_parallel) {
        size_t end = std::min(i + max_parallel, requests.size());
        for (size_t j = i; j < end; ++j) {
            results.push_back(generate_summary(requests[j]));
        }
    }

    return results;
}

void CommunitySummarizer::update_config(const Config& config) {
    config_ = config;
}

std::string CommunitySummarizer::build_summary_prompt(
    const Community& community,
    const KnowledgeGraph& graph,
    const std::string& context) {

    std::ostringstream ss;
    ss << R"(
请为以下知识图谱社区生成简洁的摘要。

社区 ID: )" << community.id << R"(

社区内实体:
)";

    // 列出实体信息
    for (const auto& entity_id : community.entity_ids) {
        // 简化: 直接使用 ID
        // 实际应该从图中获取实体详情
        ss << "- " << entity_id << "\n";
    }

    if (config_.include_relations) {
        ss << "\n社区内关系:\n";
        for (const auto& e1 : community.entity_ids) {
            for (const auto& e2 : community.entity_ids) {
                if (e1 < e2) {  // 避免重复
                    // 简化: 显示为 e1 -- e2
                    ss << "- " << e1 << " -- " << e2 << "\n";
                }
            }
        }
    }

    ss << R"(
上下文: )" << (context.empty() ? "无" : context) << R"(

请生成一个简短摘要，包含:
1. 社区主题 (1-2 句话)
2. 主要实体 (最多 )" << config_.max_examples << R"( 个)
3. 关键关系

请用以下 JSON 格式返回:
{
  "summary": "社区摘要",
  "main_themes": ["主题1", "主题2"],
  "key_entities": ["实体1", "实体2"],
  "key_relations": ["关系1", "关系2"],
  "confidence": 0.0-1.0
}

只返回 JSON。
)";

    return ss.str();
}

CommunitySummaryResult CommunitySummarizer::parse_summary_response(
    const std::string& response,
    const std::string& community_id) {

    CommunitySummaryResult result;
    result.community_id = community_id;

    // 简单 JSON 解析
    std::regex summary_regex(R"("summary"\s*:\s*"([^"]*)")");
    std::regex themes_regex(R"("main_themes"\s*:\s*\[([^\]]*)\])");
    std::regex entities_regex(R"("key_entities"\s*:\s*\[([^\]]*)\])");
    std::regex conf_regex(R"("confidence"\s*:\s*(\d+\.?\d*))");

    std::smatch match;

    if (std::regex_search(response, match, summary_regex)) {
        result.summary = match[1].str();
    }

    if (std::regex_search(response, match, conf_regex)) {
        try {
            result.confidence = std::stof(match[1].str());
        } catch (...) {}
    }

    // 解析数组
    if (std::regex_search(response, match, themes_regex)) {
        std::string themes_str = match[1].str();
        std::regex item_regex(R"("([^"]+)")");
        std::sregex_iterator it(themes_str.begin(), themes_str.end(), item_regex);
        while (it != std::sregex_iterator()) {
            result.main_themes.push_back((*it)[1].str());
            ++it;
        }
    }

    if (std::regex_search(response, match, entities_regex)) {
        std::string entities_str = match[1].str();
        std::regex item_regex(R"("([^"]+)")");
        std::sregex_iterator it(entities_str.begin(), entities_str.end(), item_regex);
        while (it != std::sregex_iterator()) {
            result.key_entities.push_back((*it)[1].str());
            ++it;
        }
    }

    return result;
}

std::vector<std::pair<std::string, std::string>> CommunitySummarizer::extract_entity_info(
    const Community& community,
    const KnowledgeGraph& graph,
    int max_count) {

    std::vector<std::pair<std::string, std::string>> info;

    int count = 0;
    for (const auto& entity_id : community.entity_ids) {
        if (count >= max_count) break;

        // 从图中查找实体
        for (const auto& entity : graph.entities) {
            if (entity.id == entity_id) {
                info.push_back({entity.name, entity.type});
                count++;
                break;
            }
        }
    }

    return info;
}

// ========== GraphRAGEnhancer ==========

GraphRAGEnhancer::GraphRAGEnhancer(
    std::shared_ptr<CommunityDetector> detector,
    std::shared_ptr<CommunitySummarizer> summarizer)
    : detector_(detector), summarizer_(summarizer) {}

bool GraphRAGEnhancer::build_community_index(
    const KnowledgeGraph& graph,
    bool build_summaries) {

    if (!detector_) {
        RAG_ERROR("Community detector not available");
        return false;
    }

    // 检测社区
    auto result = detector_->detect_communities(graph, "louvain");
    if (!result.success) {
        return false;
    }

    communities_ = result.communities;
    max_level_ = result.total_levels;

    // 生成摘要
    if (build_summaries && summarizer_) {
        for (const auto& community : communities_) {
            CommunitySummaryRequest request;
            request.community_id = community.id;
            request.graph = &graph;

            auto summary = summarizer_->generate_summary(request);
            summaries_[community.id] = summary;
        }
    }

    last_update_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    return true;
}

std::vector<Community> GraphRAGEnhancer::retrieve_communities(
    const std::string& query,
    int top_k) {

    // 过滤相关社区
    auto relevant = filter_relevant_communities(query, communities_);

    // 按相关度排序 (简化: 使用社区大小)
    std::sort(relevant.begin(), relevant.end(),
              [](const Community& a, const Community& b) {
                  return a.size > b.size;
              });

    if (relevant.size() > static_cast<size_t>(top_k)) {
        relevant.resize(top_k);
    }

    return relevant;
}

std::vector<CommunitySummaryResult> GraphRAGEnhancer::get_relevant_summaries(
    const std::string& query,
    int top_k) {

    auto relevant_communities = retrieve_communities(query, top_k);
    std::vector<CommunitySummaryResult> result;

    for (const auto& community : relevant_communities) {
        auto it = summaries_.find(community.id);
        if (it != summaries_.end()) {
            result.push_back(it->second);
        }
    }

    return result;
}

std::vector<RetrievalChunk> GraphRAGEnhancer::get_query_context(
    const std::string& query,
    const KnowledgeGraph& graph) {

    auto relevant_communities = retrieve_communities(query, 5);
    return build_context_chunks(relevant_communities, graph);
}

std::vector<Community> GraphRAGEnhancer::filter_relevant_communities(
    const std::string& query,
    const std::vector<Community>& communities) {

    // 简化: 基于关键词匹配
    std::vector<Community> relevant;

    std::string query_lower = query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& community : communities) {
        // 检查摘要是否包含查询词
        bool match = false;

        if (!community.summary.empty()) {
            std::string summary_lower = community.summary;
            std::transform(summary_lower.begin(), summary_lower.end(), summary_lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            // 简单匹配: 查询词出现在摘要中
            for (const auto& word : {"技术", "公司", "产品", "研究", "发展", "技术", "system"}) {
                if (query_lower.find(word) != std::string::npos &&
                    summary_lower.find(word) != std::string::npos) {
                    match = true;
                    break;
                }
            }
        }

        if (match) {
            relevant.push_back(community);
        }
    }

    // 如果没有匹配，返回所有
    if (relevant.empty()) {
        relevant = communities;
    }

    return relevant;
}

std::vector<RetrievalChunk> GraphRAGEnhancer::build_context_chunks(
    const std::vector<Community>& communities,
    const KnowledgeGraph& graph) {

    std::vector<RetrievalChunk> chunks;

    for (const auto& community : communities) {
        RetrievalChunk chunk;

        // 优先使用摘要
        auto it = summaries_.find(community.id);
        if (it != summaries_.end()) {
            chunk.content = it->second.summary;
        } else {
            // 构建基础内容
            std::ostringstream ss;
            ss << "Community " << community.id << ": ";
            for (size_t i = 0; i < community.entity_ids.size() && i < 10; ++i) {
                if (i > 0) ss << ", ";
                ss << community.entity_ids[i];
            }
            chunk.content = ss.str();
        }

        chunk.metadata["community_id"] = community.id;
        chunk.metadata["level"] = std::to_string(community.level);
        chunk.metadata["size"] = std::to_string(community.size);

        chunks.push_back(chunk);
    }

    return chunks;
}

// ========== Factory ==========

std::shared_ptr<CommunityDetector> create_community_detector(const CommunityDetector::Config& config) {
    return std::make_shared<CommunityDetector>(config);
}

std::shared_ptr<CommunitySummarizer> create_community_summarizer(
    std::shared_ptr<LLMService> llm_service,
    const CommunitySummarizer::Config& config) {

    auto summarizer = std::make_shared<CommunitySummarizer>(llm_service);
    summarizer->update_config(config);
    return summarizer;
}

std::shared_ptr<GraphRAGEnhancer> create_graph_rag_enhancer(
    std::shared_ptr<CommunityDetector> detector,
    std::shared_ptr<CommunitySummarizer> summarizer,
    std::shared_ptr<LLMService> llm_service) {

    if (!detector) {
        detector = create_community_detector();
    }

    if (!summarizer && llm_service) {
        summarizer = create_community_summarizer(llm_service);
    }

    return std::make_shared<GraphRAGEnhancer>(detector, summarizer);
}

}  // namespace rag
