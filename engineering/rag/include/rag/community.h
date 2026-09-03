#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace rag {

// ========== 社区信息 ==========

struct Community {
    std::string id;
    std::string name;
    std::vector<std::string> entity_ids;
    std::vector<std::string> relation_ids;
    std::string theme;
    int size = 0;
};

struct CommunitySummary {
    std::string community_id;
    std::string summary;
    std::vector<std::string> key_entities;
    std::vector<std::string> key_relations;
    int relevance_score = 0;
};

// ========== 社区检测器 ==========

class CommunityDetector {
public:
    enum class Method {
        Louvain,
        LabelPropagation,
        Greedy
    };

    explicit CommunityDetector(Method method = Method::Louvain);

    // 检测社区
    std::vector<Community> detect(
        const std::vector<std::string>& entity_ids,
        const std::unordered_map<std::string, std::vector<std::string>>& relations);

    void set_min_community_size(int size) { min_size_ = size; }
    void set_max_community_size(int size) { max_size_ = size; }

private:
    Method method_;
    int min_size_ = 3;
    int max_size_ = 100;
};

// ========== 全局上下文生成器 ==========

class GlobalContextGenerator {
public:
    struct GlobalContextResult {
        std::vector<CommunitySummary> summaries;
        std::string global_knowledge;
        std::vector<std::string> related_communities;
    };

    GlobalContextGenerator();

    // 生成全局上下文
    GlobalContextResult generate(const std::string& query);

    // 生成社区摘要
    CommunitySummary generate_community_summary(const Community& community);

    // 查找相关社区
    std::vector<std::string> find_related_communities(const std::string& query);

private:
    std::shared_ptr<CommunityDetector> detector_;
    std::unordered_map<std::string, Community> communities_cache_;
};

// ========== 工厂函数 ==========

std::unique_ptr<CommunityDetector> create_community_detector(
    CommunityDetector::Method method = CommunityDetector::Method::Louvain);

std::unique_ptr<GlobalContextGenerator> create_global_context_generator();

}  // namespace rag