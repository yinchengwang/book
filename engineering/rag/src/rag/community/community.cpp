/**
 * @file community.cpp
 * @brief 社区摘要和全局上下文生成实现
 */

#include "rag/community.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <queue>
#include <random>
#include <sstream>
#include <unordered_set>

namespace rag {

// ========== 辅助函数 ==========

namespace {

/**
 * @brief 从实体列表创建社区对象
 */
Community create_community_from_entities(
    const std::vector<std::string>& entities,
    size_t index) {
    Community community;
    community.id = "community_" + std::to_string(index);
    community.name = "Community " + std::to_string(index);
    community.entity_ids = entities;
    community.size = static_cast<int>(entities.size());

    // 简单主题提取：取前3个实体 ID 作为主题
    std::ostringstream theme;
    for (size_t i = 0; i < std::min(size_t(3), entities.size()); ++i) {
        if (i > 0) theme << ", ";
        theme << entities[i];
    }
    community.theme = theme.str();

    return community;
}

}  // anonymous namespace

// ========== CommunityDetector ==========

CommunityDetector::CommunityDetector(Method method) : method_(method) {}

std::vector<Community> CommunityDetector::detect(
    const std::vector<std::string>& entity_ids,
    const std::unordered_map<std::string, std::vector<std::string>>& relations) {

    std::vector<Community> communities;

    if (entity_ids.empty()) {
        return communities;
    }

    // 构建邻接表
    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    for (const auto& entity_id : entity_ids) {
        adjacency[entity_id] = {};
    }
    for (const auto& [source, targets] : relations) {
        for (const auto& target : targets) {
            adjacency[source].push_back(target);
            adjacency[target].push_back(source);  // 无向图
        }
    }

    // 找连通分量作为初始社区
    std::unordered_set<std::string> visited;
    std::vector<std::vector<std::string>> components;

    for (const auto& entity_id : entity_ids) {
        if (visited.find(entity_id) != visited.end()) {
            continue;
        }

        // BFS 找连通分量
        std::vector<std::string> component;
        std::queue<std::string> q;
        q.push(entity_id);
        visited.insert(entity_id);

        while (!q.empty()) {
            std::string current = q.front();
            q.pop();
            component.push_back(current);

            auto it = adjacency.find(current);
            if (it != adjacency.end()) {
                for (const auto& neighbor : it->second) {
                    if (visited.find(neighbor) == visited.end()) {
                        visited.insert(neighbor);
                        q.push(neighbor);
                    }
                }
            }
        }

        if (component.size() >= static_cast<size_t>(min_size_)) {
            components.push_back(component);
        }
    }

    // 合并小社区
    std::vector<Community> merged_communities;
    std::vector<std::vector<std::string>> small_communities;

    for (auto& component : components) {
        if (static_cast<int>(component.size()) > max_size_) {
            // 拆分大社区
            int num_splits = (static_cast<int>(component.size()) + max_size_ - 1) / max_size_;
            int split_size = static_cast<int>(component.size()) / num_splits;

            for (int i = 0; i < num_splits; ++i) {
                std::vector<std::string> split;
                int start = i * split_size;
                int end = (i == num_splits - 1) ? static_cast<int>(component.size()) : (i + 1) * split_size;
                for (int j = start; j < end; ++j) {
                    split.push_back(component[j]);
                }
                if (static_cast<int>(split.size()) >= min_size_) {
                    merged_communities.push_back(::rag::create_community_from_entities(split, merged_communities.size()));
                }
            }
        } else if (static_cast<int>(component.size()) < min_size_) {
            small_communities.push_back(component);
        } else {
            merged_communities.push_back(::rag::create_community_from_entities(component, merged_communities.size()));
        }
    }

    // 合并小社区到最近的社区
    for (auto& small : small_communities) {
        bool merged = false;
        for (auto& community : merged_communities) {
            // 检查是否有共享实体
            std::unordered_set<std::string> comm_set(community.entity_ids.begin(), community.entity_ids.end());
            for (const auto& entity : small) {
                if (comm_set.find(entity) != comm_set.end()) {
                    // 合并
                    community.entity_ids.insert(community.entity_ids.end(), small.begin(), small.end());
                    community.size = static_cast<int>(community.entity_ids.size());
                    merged = true;
                    break;
                }
            }
            if (merged) break;
        }
        if (!merged && !merged_communities.empty()) {
            // 合并到最后一个社区
            merged_communities.back().entity_ids.insert(
                merged_communities.back().entity_ids.end(),
                small.begin(), small.end()
            );
            merged_communities.back().size = static_cast<int>(merged_communities.back().entity_ids.size());
        }
    }

    return merged_communities;
}

// ========== GlobalContextGenerator ==========

GlobalContextGenerator::GlobalContextGenerator() {
    detector_ = std::make_shared<CommunityDetector>(CommunityDetector::Method::Louvain);
}

GlobalContextGenerator::GlobalContextResult GlobalContextGenerator::generate(const std::string& query) {
    GlobalContextResult result;

    // 查找相关社区
    result.related_communities = find_related_communities(query);

    // 生成相关社区的摘要
    for (const auto& comm_id : result.related_communities) {
        Community community;
        community.id = comm_id;
        auto summary = generate_community_summary(community);
        result.summaries.push_back(summary);
    }

    // 生成全局知识摘要
    std::ostringstream oss;
    oss << "Global knowledge about: " << query << ". ";
    oss << "Found " << result.summaries.size() << " related communities. ";
    for (const auto& summary : result.summaries) {
        oss << summary.summary << " ";
    }
    result.global_knowledge = oss.str();

    return result;
}

CommunitySummary GlobalContextGenerator::generate_community_summary(const Community& community) {
    CommunitySummary summary;
    summary.community_id = community.id;

    // Mock LLM 生成摘要
    std::ostringstream oss;
    oss << "Community " << community.id << " contains " << community.size << " entities. ";
    if (!community.theme.empty()) {
        oss << "Theme: " << community.theme << ". ";
    }

    // 选择关键实体（前5个）
    for (size_t i = 0; i < std::min(size_t(5), community.entity_ids.size()); ++i) {
        summary.key_entities.push_back(community.entity_ids[i]);
    }

    summary.summary = oss.str();
    summary.relevance_score = community.size;

    return summary;
}

std::vector<std::string> GlobalContextGenerator::find_related_communities(const std::string& query) {
    std::vector<std::string> related;

    // 简单的关键词匹配
    // 在实际实现中，这里会使用嵌入向量相似度或 LLM 来判断相关性
    std::string query_lower = query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // 检查缓存的社区
    for (const auto& [comm_id, community] : communities_cache_) {
        std::string theme_lower = community.theme;
        std::transform(theme_lower.begin(), theme_lower.end(), theme_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        // 简单匹配：查询词出现在主题中
        if (theme_lower.find(query_lower) != std::string::npos) {
            related.push_back(comm_id);
        }
    }

    // 如果没有匹配，返回默认社区 ID
    if (related.empty()) {
        related.push_back("community_0");
        related.push_back("community_1");
    }

    return related;
}

// ========== 工厂函数 ==========

std::unique_ptr<CommunityDetector> create_community_detector(CommunityDetector::Method method) {
    return std::make_unique<CommunityDetector>(method);
}

std::unique_ptr<GlobalContextGenerator> create_global_context_generator() {
    return std::make_unique<GlobalContextGenerator>();
}

}  // namespace rag
