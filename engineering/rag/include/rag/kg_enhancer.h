#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "rag/types.h"

namespace rag {

// ========== 实体信息 ==========

struct Entity {
    std::string id;
    std::string name;
    std::string type;          // person, location, organization, concept
    std::string description;
    std::vector<std::string> aliases;
    std::unordered_map<std::string, std::string> properties;
    float confidence = 1.0f;
};

// ========== 关系信息 ==========

struct Relation {
    std::string id;
    std::string source_id;
    std::string target_id;
    std::string type;          // works_at, located_in, created_by, etc.
    float confidence = 1.0f;
};

// ========== 实体链接结果 ==========

struct EntityLinkingResult {
    std::string text;          // 原文
    std::string entity_id;      // 链接到的实体 ID
    std::string entity_name;   // 实体名称
    std::string entity_type;   // 实体类型
    int start_pos;             // 在原文中的位置
    int end_pos;
    float confidence;
    std::string source;        // "knowledge_graph", "ner", "disambiguation"
};

// ========== 实体链接器 ==========

class EntityLinker {
public:
    EntityLinker();

    // 链接文本中的实体
    std::vector<EntityLinkingResult> link(const std::string& text);

    // 链接单个实体
    EntityLinkingResult link_entity(
        const std::string& entity_name,
        const std::string& context = "");

    // 添加候选实体
    void add_candidate(const Entity& entity);

    // 设置知识图谱
    void set_knowledge_graph(std::shared_ptr<class KnowledgeGraph> kg);

private:
    // NER 模型（mock）
    std::vector<std::pair<std::string, std::string>> extract_mentions(
        const std::string& text);

    // 消歧
    EntityLinkingResult disambiguate(
        const std::string& mention,
        const std::string& context,
        const std::vector<Entity>& candidates);

    std::shared_ptr<class KnowledgeGraph> kg_;
    std::vector<Entity> candidates_;
};

// ========== 图谱增强检索 ==========

class KGEnhancedRetriever {
public:
    KGEnhancedRetriever(
        std::shared_ptr<class RetrievalPipeline> base_retriever,
        std::shared_ptr<class KnowledgeGraph> kg,
        std::shared_ptr<EntityLinker> linker);

    // 检索
    RetrievalResult retrieve(const std::string& query, int top_k);

    // 展开多跳查询
    std::vector<std::string> expand_multihop(
        const std::string& query,
        int max_hops = 2);

    // 获取实体上下文
    std::string get_entity_context(const std::string& entity_name);

private:
    std::shared_ptr<class RetrievalPipeline> base_retriever_;
    std::shared_ptr<class KnowledgeGraph> kg_;
    std::shared_ptr<EntityLinker> linker_;

    // 生成子查询
    std::vector<std::string> generate_subqueries(
        const std::string& entity,
        int hop);
};

// ========== 知识图谱构建器 ==========

class KGBuilder {
public:
    KGBuilder();

    // 从文档构建
    void build_from_documents(
        const std::vector<std::string>& documents);

    // 添加实体
    void add_entity(const Entity& entity);

    // 添加关系
    void add_relation(const Relation& relation);

    // 获取知识图谱
    std::shared_ptr<class KnowledgeGraph> build();

    // 保存到文件
    bool save(const std::string& path);

    // 从文件加载
    bool load(const std::string& path);

private:
    std::shared_ptr<class KnowledgeGraph> kg_;
    std::shared_ptr<EntityLinker> linker_;
    std::vector<Entity> entities_;
    std::vector<Relation> relations_;
};

// ========== 工厂函数 ==========

std::unique_ptr<EntityLinker> create_entity_linker(
    std::shared_ptr<class KnowledgeGraph> kg = nullptr);

std::unique_ptr<KGEnhancedRetriever> create_kg_enhanced_retriever(
    std::shared_ptr<class RetrievalPipeline> base_retriever,
    std::shared_ptr<class KnowledgeGraph> kg);

std::unique_ptr<KGBuilder> create_kg_builder();

}  // namespace rag
