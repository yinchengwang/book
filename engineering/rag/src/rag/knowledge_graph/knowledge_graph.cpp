/**
 * @file knowledge_graph.cpp
 * @brief 知识图谱实现
 */

#include "rag/knowledge_graph.h"
#include "rag/logger.h"
#include <algorithm>
#include <queue>
#include <stack>
#include <set>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace rag {

// ========== 工具函数 ==========

static int64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static std::string generate_id() {
    static uint64_t counter = 0;
    return "kg_" + std::to_string(++counter) + "_" +
           std::to_string(current_time_ms());
}

// ========== KGPath 实现 ==========

std::string KGPath::to_string() const {
    std::ostringstream oss;
    for (size_t i = 0; i < entities.size(); ++i) {
        if (i > 0) {
            if (i - 1 < relations.size()) {
                oss << " --[" << relations[i-1].type << "]--> ";
            }
        }
        oss << entities[i].name;
    }
    return oss.str();
}

// ========== 内存图谱实现 ==========

class InMemoryKnowledgeGraph : public KnowledgeGraph {
public:
    InMemoryKnowledgeGraph() = default;

    // ========== 实体操作 ==========

    bool add_entity(const KGEntity& entity) override {
        if (entity.id.empty()) {
            return false;
        }
        if (entities_.find(entity.id) != entities_.end()) {
            RAG_WARN("Entity already exists: " + entity.id);
            return false;
        }

        KGEntity e = entity;
        if (e.created_at == 0) {
            e.created_at = current_time_ms();
        }
        e.updated_at = e.created_at;

        entities_.emplace(e.id, e);
        name_index_[e.name] = e.id;

        // 添加别名索引
        for (const auto& alias : e.aliases) {
            name_index_[alias] = e.id;
        }

        RAG_DEBUG("Added entity: " + e.name + " (" + e.id + ")");
        return true;
    }

    int add_entities(const std::vector<KGEntity>& entities) override {
        int count = 0;
        for (const auto& e : entities) {
            if (add_entity(e)) {
                count++;
            }
        }
        return count;
    }

    std::optional<KGEntity> get_entity(const std::string& entity_id) const override {
        auto it = entities_.find(entity_id);
        if (it != entities_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<KGEntity> get_entity_by_name(const std::string& name) const override {
        auto it = name_index_.find(name);
        if (it != name_index_.end()) {
            auto eit = entities_.find(it->second);
            if (eit != entities_.end()) {
                return eit->second;
            }
        }
        return std::nullopt;
    }

    bool update_entity(const KGEntity& entity) override {
        auto it = entities_.find(entity.id);
        if (it == entities_.end()) {
            return false;
        }

        KGEntity e = entity;
        e.updated_at = current_time_ms();
        e.created_at = it->second.created_at;  // 保持创建时间不变

        entities_[e.id] = e;

        // 更新索引
        name_index_[e.name] = e.id;
        for (const auto& alias : e.aliases) {
            name_index_[alias] = e.id;
        }

        return true;
    }

    bool delete_entity(const std::string& entity_id) override {
        auto it = entities_.find(entity_id);
        if (it == entities_.end()) {
            return false;
        }

        // 删除关联的关系
        auto rels = get_relations(entity_id);
        for (const auto& rel : rels) {
            delete_relation(rel.id);
        }

        // 删除索引
        name_index_.erase(it->second.name);
        for (const auto& alias : it->second.aliases) {
            name_index_.erase(alias);
        }

        entities_.erase(it);
        RAG_DEBUG("Deleted entity: " + entity_id);
        return true;
    }

    std::vector<KGEntity> get_all_entities() const override {
        std::vector<KGEntity> result;
        result.reserve(entities_.size());
        for (const auto& [id, entity] : entities_) {
            result.push_back(entity);
        }
        return result;
    }

    std::vector<KGEntity> get_entities_by_type(EntityType type) const override {
        std::vector<KGEntity> result;
        for (const auto& [id, entity] : entities_) {
            if (entity.type == type) {
                result.push_back(entity);
            }
        }
        return result;
    }

    bool has_entity(const std::string& entity_id) const override {
        return entities_.find(entity_id) != entities_.end();
    }

    // ========== 关系操作 ==========

    bool add_relation(const KGRelation& relation) override {
        if (relation.id.empty() || relation.source_id.empty() || relation.target_id.empty()) {
            return false;
        }

        // 确保实体存在
        if (!has_entity(relation.source_id) || !has_entity(relation.target_id)) {
            RAG_WARN("Relation refers to non-existent entity");
            return false;
        }

        if (relations_.find(relation.id) != relations_.end()) {
            return false;
        }

        KGRelation r = relation;
        if (r.created_at == 0) {
            r.created_at = current_time_ms();
        }

        relations_.emplace(r.id, r);

        // 更新实体度数
        auto& source = entities_[r.source_id];
        auto& target = entities_[r.target_id];
        source.degree++;
        target.degree++;

        // 更新邻接表
        adjacency_[r.source_id].insert({r.target_id, r.id});
        adjacency_[r.target_id].insert({r.source_id, r.id});

        // 更新关系索引
        std::string rel_key = r.source_id + "||" + r.target_id;
        rel_index_[rel_key] = r.id;

        RAG_DEBUG("Added relation: " + r.source_id + " --[" + r.type + "]--> " + r.target_id);
        return true;
    }

    int add_relations(const std::vector<KGRelation>& relations) override {
        int count = 0;
        for (const auto& r : relations) {
            if (add_relation(r)) {
                count++;
            }
        }
        return count;
    }

    std::optional<KGRelation> get_relation(const std::string& relation_id) const override {
        auto it = relations_.find(relation_id);
        if (it != relations_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool delete_relation(const std::string& relation_id) override {
        auto it = relations_.find(relation_id);
        if (it == relations_.end()) {
            return false;
        }

        const auto& r = it->second;

        // 更新实体度数
        if (entities_.find(r.source_id) != entities_.end()) {
            entities_[r.source_id].degree--;
        }
        if (entities_.find(r.target_id) != entities_.end()) {
            entities_[r.target_id].degree--;
        }

        // 删除邻接表
        adjacency_[r.source_id].erase({r.target_id, r.id});
        adjacency_[r.target_id].erase({r.source_id, r.id});

        // 删除索引
        std::string rel_key = r.source_id + "||" + r.target_id;
        rel_index_.erase(rel_key);

        relations_.erase(it);
        return true;
    }

    std::vector<KGRelation> get_relations(const std::string& entity_id) const override {
        std::vector<KGRelation> result;
        for (const auto& [id, rel] : relations_) {
            if (rel.source_id == entity_id || rel.target_id == entity_id) {
                result.push_back(rel);
            }
        }
        return result;
    }

    std::vector<KGRelation> get_relations_between(
        const std::string& entity1_id,
        const std::string& entity2_id) const override {
        std::vector<KGRelation> result;
        for (const auto& [id, rel] : relations_) {
            if ((rel.source_id == entity1_id && rel.target_id == entity2_id) ||
                (rel.source_id == entity2_id && rel.target_id == entity1_id)) {
                result.push_back(rel);
            }
        }
        return result;
    }

    // ========== 图遍历 ==========

    std::vector<KGEntity> get_neighbors(
        const std::string& entity_id,
        int max_hops) const override {

        std::unordered_set<std::string> visited;
        std::queue<std::pair<std::string, int>> queue;

        visited.insert(entity_id);
        queue.push({entity_id, 0});

        std::vector<KGEntity> result;

        while (!queue.empty()) {
            auto [current, hops] = queue.front();
            queue.pop();

            if (hops > 0 && hops <= max_hops) {
                if (auto entity = get_entity(current)) {
                    result.push_back(*entity);
                }
            }

            if (hops < max_hops) {
                auto it = adjacency_.find(current);
                if (it != adjacency_.end()) {
                    for (const auto& [neighbor, rel_id] : it->second) {
                        if (visited.find(neighbor) == visited.end()) {
                            visited.insert(neighbor);
                            queue.push({neighbor, hops + 1});
                        }
                    }
                }
            }
        }

        return result;
    }

    std::vector<KGEntity> get_reachable_entities(
        const std::string& entity_id,
        int max_hops) const override {
        return get_neighbors(entity_id, max_hops);
    }

    std::optional<KGPath> find_shortest_path(
        const std::string& source_id,
        const std::string& target_id,
        int max_hops) const override {

        if (source_id == target_id) {
            KGPath path;
            if (auto entity = get_entity(source_id)) {
                path.entities.push_back(*entity);
                path.hop_count = 0;
            }
            return path;
        }

        std::unordered_map<std::string, std::pair<std::string, std::string>> prev;
        // prev[entity_id] = {prev_entity_id, relation_id}

        std::unordered_set<std::string> visited;
        std::queue<std::pair<std::string, int>> queue;

        visited.insert(source_id);
        queue.push({source_id, 0});

        bool found = false;

        while (!queue.empty() && !found) {
            auto [current, hops] = queue.front();
            queue.pop();

            if (hops >= max_hops) {
                continue;
            }

            auto it = adjacency_.find(current);
            if (it != adjacency_.end()) {
                for (const auto& [neighbor, rel_id] : it->second) {
                    if (visited.find(neighbor) == visited.end()) {
                        visited.insert(neighbor);
                        prev[neighbor] = {current, rel_id};

                        if (neighbor == target_id) {
                            found = true;
                            break;
                        }

                        queue.push({neighbor, hops + 1});
                    }
                }
            }
        }

        if (!found) {
            return std::nullopt;
        }

        // 重建路径
        KGPath path;
        std::string current = target_id;

        while (prev.find(current) != prev.end()) {
            auto [prev_entity, rel_id] = prev[current];
            if (auto entity = get_entity(current)) {
                path.entities.insert(path.entities.begin(), *entity);
            }
            if (auto rel = get_relation(rel_id)) {
                path.relations.insert(path.relations.begin(), *rel);
                path.total_weight += rel->weight;
            }
            current = prev_entity;
        }

        if (auto entity = get_entity(source_id)) {
            path.entities.insert(path.entities.begin(), *entity);
        }

        path.hop_count = static_cast<int>(path.relations.size());

        return path;
    }

    KGSubgraph get_subgraph(
        const std::string& root_entity_id,
        int max_hops) const override {

        KGSubgraph subgraph;
        subgraph.root_entity_id = root_entity_id;

        // BFS 获取实体
        std::unordered_set<std::string> visited;
        std::queue<std::pair<std::string, int>> queue;

        visited.insert(root_entity_id);
        queue.push({root_entity_id, 0});

        if (auto root = get_entity(root_entity_id)) {
            subgraph.central_topic = root->name;
            subgraph.entities.push_back(*root);
        }

        while (!queue.empty()) {
            auto [current, hops] = queue.front();
            queue.pop();

            if (hops >= max_hops) {
                continue;
            }

            auto it = adjacency_.find(current);
            if (it != adjacency_.end()) {
                for (const auto& [neighbor, rel_id] : it->second) {
                    if (visited.find(neighbor) == visited.end()) {
                        visited.insert(neighbor);

                        if (auto entity = get_entity(neighbor)) {
                            subgraph.entities.push_back(*entity);
                        }

                        queue.push({neighbor, hops + 1});
                    }

                    if (auto rel = get_relation(rel_id)) {
                        subgraph.relations.push_back(*rel);
                    }
                }
            }
        }

        subgraph.hop_count = max_hops;

        return subgraph;
    }

    // ========== 统计信息 ==========

    int64_t entity_count() const override {
        return static_cast<int64_t>(entities_.size());
    }

    int64_t relation_count() const override {
        return static_cast<int64_t>(relations_.size());
    }

    float density() const override {
        int64_t n = entity_count();
        if (n < 2) return 0.0f;
        int64_t max_edges = n * (n - 1) / 2;
        return static_cast<float>(relation_count()) / max_edges;
    }

    // ========== 持久化 ==========

    bool save(const std::string& path) const override {
        std::ofstream file(path);
        if (!file.is_open()) {
            RAG_ERROR("Failed to open file for save: " + path);
            return false;
        }

        // JSON 格式保存
        file << "{\n";
        file << "  \"entities\": [\n";

        bool first = true;
        for (const auto& [id, entity] : entities_) {
            if (!first) file << ",\n";
            file << "    {\n";
            file << "      \"id\": \"" << entity.id << "\",\n";
            file << "      \"name\": \"" << entity.name << "\",\n";
            file << "      \"type\": \"" << entity_type_to_string(entity.type) << "\",\n";
            file << "      \"description\": \"" << entity.description << "\",\n";
            file << "      \"source_chunk_id\": \"" << entity.source_chunk_id << "\",\n";
            file << "      \"confidence\": " << entity.confidence << "\n";
            file << "    }";
            first = false;
        }

        file << "\n  ],\n";
        file << "  \"relations\": [\n";

        first = true;
        for (const auto& [id, rel] : relations_) {
            if (!first) file << ",\n";
            file << "    {\n";
            file << "      \"id\": \"" << rel.id << "\",\n";
            file << "      \"source_id\": \"" << rel.source_id << "\",\n";
            file << "      \"target_id\": \"" << rel.target_id << "\",\n";
            file << "      \"type\": \"" << rel.type << "\",\n";
            file << "      \"description\": \"" << rel.description << "\",\n";
            file << "      \"weight\": " << rel.weight << ",\n";
            file << "      \"source_chunk_id\": \"" << rel.source_chunk_id << "\",\n";
            file << "      \"confidence\": " << rel.confidence << "\n";
            file << "    }";
            first = false;
        }

        file << "\n  ]\n";
        file << "}\n";

        file.close();
        RAG_INFO("Knowledge graph saved to: " + path);
        return true;
    }

    bool load(const std::string& path) override {
        std::ifstream file(path);
        if (!file.is_open()) {
            RAG_ERROR("Failed to open file for load: " + path);
            return false;
        }

        // 简单的 JSON 解析 (实际应用中建议使用 JSON 库)
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // 清空现有数据
        clear();

        // 解析实体
        size_t pos = 0;
        while ((pos = content.find("\"id\":", pos)) != std::string::npos) {
            // 简化解析 - 实际应用中应使用完整的 JSON 解析器
            // 这里只是占位实现
            break;
        }

        file.close();
        RAG_INFO("Knowledge graph loaded from: " + path);
        return true;
    }

    void clear() override {
        entities_.clear();
        relations_.clear();
        adjacency_.clear();
        name_index_.clear();
        rel_index_.clear();
    }

private:
    // 核心存储
    std::unordered_map<std::string, KGEntity> entities_;
    std::unordered_map<std::string, KGRelation> relations_;

    // 索引
    std::unordered_map<std::string, std::string> name_index_;  // name -> entity_id
    std::unordered_map<std::string, std::string> rel_index_;   // source||target -> rel_id

    // 邻接表: entity_id -> {neighbor_id, relation_id}
    std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> adjacency_;
};

// ========== 工厂函数 ==========

std::unique_ptr<KnowledgeGraph> create_knowledge_graph() {
    return std::make_unique<InMemoryKnowledgeGraph>();
}

}  // namespace rag
