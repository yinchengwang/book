/**
 * @file knowledge_graph.h
 * @brief 知识图谱 - 数据结构和接口
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <variant>

namespace rag {

// ========== 实体类型 ==========

/**
 * @brief 实体类型枚举
 */
enum class EntityType {
    UNKNOWN = 0,
    PERSON,           // 人物
    ORGANIZATION,     // 组织/公司
    LOCATION,         // 地点
    CONCEPT,          // 概念
    EVENT,            // 事件
    PRODUCT,          // 产品
    TECHNOLOGY,       // 技术
    TIME,             // 时间
    NUMBER,           // 数字
    CUSTOM            // 自定义
};

/**
 * @brief 实体类型转字符串
 */
inline std::string entity_type_to_string(EntityType type) {
    switch (type) {
        case EntityType::PERSON: return "PERSON";
        case EntityType::ORGANIZATION: return "ORGANIZATION";
        case EntityType::LOCATION: return "LOCATION";
        case EntityType::CONCEPT: return "CONCEPT";
        case EntityType::EVENT: return "EVENT";
        case EntityType::PRODUCT: return "PRODUCT";
        case EntityType::TECHNOLOGY: return "TECHNOLOGY";
        case EntityType::TIME: return "TIME";
        case EntityType::NUMBER: return "NUMBER";
        case EntityType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

// ========== 实体 ==========

/**
 * @brief 知识图谱实体
 */
struct KGEntity {
    std::string id;                 // 实体唯一ID
    std::string name;               // 实体名称
    EntityType type = EntityType::UNKNOWN;  // 实体类型
    std::string description;        // 实体描述
    std::map<std::string, std::string> properties;  // 属性
    std::vector<std::string> aliases;  // 别名
    std::string source_chunk_id;    // 来源 chunk
    float confidence = 1.0f;        // 置信度

    // 统计信息
    int64_t created_at = 0;
    int64_t updated_at = 0;
    int degree = 0;                 // 度数 (连接的边数)

    // Embedding 用于语义搜索
    std::vector<float> embedding;

    bool operator==(const KGEntity& other) const {
        return id == other.id;
    }
};

// ========== 关系类型 ==========

/**
 * @brief 预定义关系类型
 */
namespace RelationTypes {
    // 通用关系
    const std::string IS_A = "IS_A";
    const std::string HAS_A = "HAS_A";
    const std::string PART_OF = "PART_OF";
    const std::string LOCATED_IN = "LOCATED_IN";
    const std::string WORKS_AT = "WORKS_AT";
    const std::string WORKS_FOR = "WORKS_FOR";
    const std::string CREATED_BY = "CREATED_BY";
    const std::string CREATES = "CREATES";
    const std::string USES = "USES";
    const std::string USED_BY = "USED_BY";
    const std::string RELATED_TO = "RELATED_TO";
    const std::string SIMILAR_TO = "SIMILAR_TO";
    const std::string BEFORE = "BEFORE";
    const std::string AFTER = "AFTER";
    const std::string CAUSES = "CAUSES";
    const std::string DEPENDS_ON = "DEPENDS_ON";
}

// ========== 关系 ==========

/**
 * @brief 知识图谱关系 (边)
 */
struct KGRelation {
    std::string id;                 // 关系唯一ID
    std::string source_id;          // 源实体ID
    std::string target_id;          // 目标实体ID
    std::string type;               // 关系类型
    std::string description;        // 关系描述
    float weight = 1.0f;            // 权重
    std::map<std::string, std::string> properties;  // 属性

    // 来源信息
    std::string source_chunk_id;    // 来源 chunk
    float confidence = 1.0f;         // 置信度
    int64_t created_at = 0;

    bool operator==(const KGRelation& other) const {
        return id == other.id;
    }
};

// ========== 路径 ==========

/**
 * @brief 实体路径 (用于多跳查询)
 */
struct KGPath {
    std::vector<KGEntity> entities;     // 路径上的实体
    std::vector<KGRelation> relations;  // 路径上的关系

    float total_weight = 0.0f;         // 路径总权重
    int hop_count = 0;                  // 跳数

    std::string to_string() const;
};

// ========== 子图 ==========

/**
 * @brief 子图 (用于检索结果)
 */
struct KGSubgraph {
    std::vector<KGEntity> entities;
    std::vector<KGRelation> relations;

    // 来源信息
    std::string root_entity_id;
    std::string central_topic;

    int hop_count = 0;
    float relevance_score = 0.0f;
};

// ========== 知识图谱接口 ==========

/**
 * @brief 知识图谱接口
 */
class KnowledgeGraph {
public:
    virtual ~KnowledgeGraph() = default;

    // ========== 实体操作 ==========

    /// 添加实体
    virtual bool add_entity(const KGEntity& entity) = 0;

    /// 批量添加实体
    virtual int add_entities(const std::vector<KGEntity>& entities) = 0;

    /// 获取实体
    virtual std::optional<KGEntity> get_entity(const std::string& entity_id) const = 0;

    /// 获取实体 (按名称)
    virtual std::optional<KGEntity> get_entity_by_name(const std::string& name) const = 0;

    /// 更新实体
    virtual bool update_entity(const KGEntity& entity) = 0;

    /// 删除实体
    virtual bool delete_entity(const std::string& entity_id) = 0;

    /// 获取所有实体
    virtual std::vector<KGEntity> get_all_entities() const = 0;

    /// 按类型获取实体
    virtual std::vector<KGEntity> get_entities_by_type(EntityType type) const = 0;

    /// 实体是否存在
    virtual bool has_entity(const std::string& entity_id) const = 0;

    // ========== 关系操作 ==========

    /// 添加关系
    virtual bool add_relation(const KGRelation& relation) = 0;

    /// 批量添加关系
    virtual int add_relations(const std::vector<KGRelation>& relations) = 0;

    /// 获取关系
    virtual std::optional<KGRelation> get_relation(const std::string& relation_id) const = 0;

    /// 删除关系
    virtual bool delete_relation(const std::string& relation_id) = 0;

    /// 获取实体的所有关系
    virtual std::vector<KGRelation> get_relations(const std::string& entity_id) const = 0;

    /// 获取两个实体间的关系
    virtual std::vector<KGRelation> get_relations_between(
        const std::string& entity1_id,
        const std::string& entity2_id) const = 0;

    // ========== 图遍历 ==========

    /// 获取邻居实体 (1跳)
    virtual std::vector<KGEntity> get_neighbors(
        const std::string& entity_id,
        int max_hops = 1) const = 0;

    /// 获取 N 跳内的所有实体
    virtual std::vector<KGEntity> get_reachable_entities(
        const std::string& entity_id,
        int max_hops = 2) const = 0;

    /// 查找两个实体间的最短路径
    virtual std::optional<KGPath> find_shortest_path(
        const std::string& source_id,
        const std::string& target_id,
        int max_hops = 5) const = 0;

    /// 获取子图
    virtual KGSubgraph get_subgraph(
        const std::string& root_entity_id,
        int max_hops = 2) const = 0;

    // ========== 统计信息 ==========

    /// 获取实体数量
    virtual int64_t entity_count() const = 0;

    /// 获取关系数量
    virtual int64_t relation_count() const = 0;

    /// 获取图的密度
    virtual float density() const = 0;

    // ========== 持久化 ==========

    /// 保存到文件
    virtual bool save(const std::string& path) const = 0;

    /// 从文件加载
    virtual bool load(const std::string& path) = 0;

    /// 清空图谱
    virtual void clear() = 0;
};

// ========== 工厂函数 ==========

std::unique_ptr<KnowledgeGraph> create_knowledge_graph();

}  // namespace rag
