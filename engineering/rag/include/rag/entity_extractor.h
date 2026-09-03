/**
 * @file entity_extractor.h
 * @brief 实体提取器 - 从文本中提取实体和关系
 */
#pragma once

#include "rag/knowledge_graph.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace rag {

// ========== 提取结果 ==========

/**
 * @brief 提取结果
 */
struct ExtractionResult {
    std::vector<KGEntity> entities;       // 提取的实体
    std::vector<KGRelation> relations;     // 提取的关系
    std::string source_chunk_id;           // 来源 chunk
    int64_t processing_time_ms = 0;       // 处理时间
    float confidence = 1.0f;               // 整体置信度
};

// ========== 实体提取器接口 ==========

/**
 * @brief 实体提取器接口
 */
class EntityExtractor {
public:
    virtual ~EntityExtractor() = default;

    /// 提取实体和关系
    virtual ExtractionResult extract(
        const std::string& text,
        const std::string& chunk_id) = 0;

    /// 批量提取
    virtual std::vector<ExtractionResult> extract_batch(
        const std::vector<std::pair<std::string, std::string>>& texts_and_ids) = 0;

    /// 获取提取器名称
    virtual std::string name() const = 0;
};

// ========== 基于规则的提取器 ==========

/**
 * @brief 基于规则的实体提取器
 *
 * 使用正则表达式和启发式规则提取实体
 */
class RuleBasedExtractor : public EntityExtractor {
public:
    RuleBasedExtractor();

    ExtractionResult extract(
        const std::string& text,
        const std::string& chunk_id) override;

    std::vector<ExtractionResult> extract_batch(
        const std::vector<std::pair<std::string, std::string>>& texts_and_ids) override;

    std::string name() const override { return "rule-based"; }

    /// 添加自定义正则规则
    void add_pattern(EntityType type, const std::string& pattern);

    /// 添加自定义关系规则
    void add_relation_pattern(const std::string& pattern, const std::string& rel_type);

    /// 设置是否启用特定实体类型
    void enable_entity_type(EntityType type, bool enable);

private:
    /// 提取实体
    std::vector<KGEntity> extract_entities(const std::string& text,
                                           const std::string& chunk_id);

    /// 提取关系
    std::vector<KGRelation> extract_relations(const std::string& text,
                                              const std::vector<KGEntity>& entities,
                                              const std::string& chunk_id);

    /// 清理实体名称
    std::string clean_entity_name(const std::string& name);

    /// 实体去重
    std::vector<KGEntity> deduplicate_entities(std::vector<KGEntity>& entities);

    /// 生成实体 ID
    std::string generate_entity_id(const std::string& name, EntityType type);

    /// 生成关系 ID
    std::string generate_relation_id(const std::string& source,
                                     const std::string& target,
                                     const std::string& type);

    // 规则定义
    struct PatternRule {
        EntityType type;
        std::string pattern;
        float weight = 1.0f;
    };

    struct RelationRule {
        std::string pattern;   // 正则模式
        std::string rel_type;  // 关系类型
        int source_idx;        // 源实体在匹配组中的索引
        int target_idx;        // 目标实体在匹配组中的索引
    };

    std::vector<PatternRule> entity_patterns_;
    std::vector<RelationRule> relation_patterns_;
    std::unordered_map<EntityType, bool> enabled_types_;
    std::unordered_set<std::string> seen_entities_;
    uint64_t entity_counter_ = 0;
    uint64_t relation_counter_ = 0;
};

// ========== 基于 LLM 的提取器 ==========

/**
 * @brief 基于 LLM 的实体提取器
 *
 * 使用 LLM 从文本中提取实体和关系
 */
class LLMBasedExtractor : public EntityExtractor {
public:
    explicit LLMBasedExtractor(void* llm_service = nullptr);

    ExtractionResult extract(
        const std::string& text,
        const std::string& chunk_id) override;

    std::vector<ExtractionResult> extract_batch(
        const std::vector<std::pair<std::string, std::string>>& texts_and_ids) override;

    std::string name() const override { return "llm-based"; }

    /// 设置 LLM 服务
    void set_llm_service(void* llm_service);

    /// 设置提示词模板
    void set_prompt_template(const std::string& template_str);

    /// 设置是否启用关系提取
    void enable_relation_extraction(bool enable) { enable_relation_ = enable; }

private:
    /// 生成 LLM 提示词
    std::string build_prompt(const std::string& text);

    /// 解析 LLM 响应
    std::vector<KGEntity> parse_entities(const std::string& response);
    std::vector<KGRelation> parse_relations(const std::string& response);

    void* llm_service_;
    std::string prompt_template_;
    bool enable_relation_ = true;
};

// ========== 工厂函数 ==========

std::unique_ptr<EntityExtractor> create_entity_extractor(
    const std::string& type = "rule-based");

}  // namespace rag
