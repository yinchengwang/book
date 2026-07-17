/**
 * @file entity_extraction.cpp
 * @brief 实体提取器实现
 */

#include "rag/entity_extractor.h"
#include "rag/logger.h"
#include <algorithm>
#include <regex>
#include <chrono>
#include <unordered_set>
#include <cstring>

namespace rag {

// ========== 工具函数 ==========

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// ========== RuleBasedExtractor 实现 ==========

RuleBasedExtractor::RuleBasedExtractor() {
    // 初始化默认的实体模式

    // 人名模式 (简单的中文/英文姓名)
    entity_patterns_.push_back({EntityType::PERSON, R"((?:[A-Z][a-z]+ ){1,2}[A-Z][a-z]+)"});  // 英文名
    entity_patterns_.push_back({EntityType::PERSON, R"([\x{4e00}-\x{9fa5}]{2,4}(?:先生|女士|教授|博士|老师))"});  // 中文称谓

    // 组织/公司模式
    entity_patterns_.push_back({EntityType::ORGANIZATION, R"((?:[A-Z][a-z]+ )+(?:公司|集团|大学|学院|研究所|医院|医院))"});  // 中文组织
    entity_patterns_.push_back({EntityType::ORGANIZATION, R"([A-Z][a-z]+(?:Corp|LLC|Inc|Group|University))"});  // 英文组织

    // 地点模式
    entity_patterns_.push_back({EntityType::LOCATION, R"((?:[\x{4e00}-\x{9fa5}]+省|[\x{4e00}-\x{9fa5}]+市|[\x{4e00}-\x{9fa5}]+县))"});  // 中国地名
    entity_patterns_.push_back({EntityType::LOCATION, R"([A-Z][a-z]+(?: City| Country| Region))"});  // 英文地点

    // 技术/产品模式
    entity_patterns_.push_back({EntityType::TECHNOLOGY, R"([A-Z][a-z]+(?:Net|DB|API|SDK|LLM|RAG|Graph))"});  // 技术缩写
    entity_patterns_.push_back({EntityType::PRODUCT, R"([A-Z][a-z]+(?:Pro|Max|Plus|Enterprise))"});  // 产品版本

    // 时间模式
    entity_patterns_.push_back({EntityType::TIME, R"(\d{4}年\d{1,2}月\d{1,2}日)"});  // 中文日期
    entity_patterns_.push_back({EntityType::TIME, R"(\d{4}-\d{2}-\d{2})"});  // ISO 日期
    entity_patterns_.push_back({EntityType::TIME, R"(\d+年)"});  // 年份

    // 默认启用所有类型
    for (int i = static_cast<int>(EntityType::UNKNOWN) + 1;
         i <= static_cast<int>(EntityType::CUSTOM); ++i) {
        enabled_types_[static_cast<EntityType>(i)] = true;
    }

    // 初始化默认关系规则
    relation_patterns_.push_back({
        R"((\w+)在(\w+)工作)",
        RelationTypes::WORKS_AT,
        1, 2
    });

    relation_patterns_.push_back({
        R"((\w+)创建了(\w+))",
        RelationTypes::CREATES,
        1, 2
    });

    relation_patterns_.push_back({
        R"((\w+)位于(\w+))",
        RelationTypes::LOCATED_IN,
        1, 2
    });

    relation_patterns_.push_back({
        R"((\w+)使用了(\w+))",
        RelationTypes::USES,
        1, 2
    });
}

ExtractionResult RuleBasedExtractor::extract(
    const std::string& text,
    const std::string& chunk_id) {

    ExtractionResult result;
    result.source_chunk_id = chunk_id;

    auto start = std::chrono::steady_clock::now();

    // 1. 提取实体
    result.entities = extract_entities(text, chunk_id);

    // 2. 提取关系
    result.relations = extract_relations(text, result.entities, chunk_id);

    // 3. 去重
    result.entities = deduplicate_entities(result.entities);

    // 4. 统计
    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    RAG_DEBUG("Extracted " + std::to_string(result.entities.size()) + " entities and "
              + std::to_string(result.relations.size()) + " relations");

    return result;
}

std::vector<ExtractionResult> RuleBasedExtractor::extract_batch(
    const std::vector<std::pair<std::string, std::string>>& texts_and_ids) {

    std::vector<ExtractionResult> results;
    results.reserve(texts_and_ids.size());

    for (const auto& [text, chunk_id] : texts_and_ids) {
        results.push_back(extract(text, chunk_id));
    }

    return results;
}

std::vector<KGEntity> RuleBasedExtractor::extract_entities(
    const std::string& text,
    const std::string& chunk_id) {

    std::vector<KGEntity> entities;

    for (const auto& rule : entity_patterns_) {
        if (!enabled_types_[rule.type]) {
            continue;
        }

        try {
            std::regex pattern(rule.pattern);
            std::sregex_iterator it(text.begin(), text.end(), pattern);
            std::sregex_iterator end;

            while (it != end) {
                std::string match = trim(it->str());

                // 清理名称
                std::string name = clean_entity_name(match);
                if (name.length() >= 2) {
                    KGEntity entity;
                    entity.id = generate_entity_id(name, rule.type);
                    entity.name = name;
                    entity.type = rule.type;
                    entity.source_chunk_id = chunk_id;
                    entity.confidence = rule.weight;

                    entities.push_back(entity);
                }

                ++it;
            }
        } catch (const std::regex_error& e) {
            RAG_WARN("Regex error: " + std::string(e.what()));
        }
    }

    return entities;
}

std::vector<KGRelation> RuleBasedExtractor::extract_relations(
    const std::string& text,
    const std::vector<KGEntity>& entities,
    const std::string& chunk_id) {

    std::vector<KGRelation> relations;

    // 构建实体名称集合用于匹配
    std::unordered_map<std::string, std::string> name_to_id;
    for (const auto& e : entities) {
        name_to_id[e.name] = e.id;
        name_to_id[to_lower(e.name)] = e.id;
    }

    for (const auto& rule : relation_patterns_) {
        try {
            std::regex pattern(rule.pattern);
            std::sregex_iterator it(text.begin(), text.end(), pattern);
            std::sregex_iterator end;

            while (it != end) {
                if (rule.source_idx < static_cast<int>(it->size()) &&
                    rule.target_idx < static_cast<int>(it->size())) {

                    std::string source_name = trim((*it)[rule.source_idx].str());
                    std::string target_name = trim((*it)[rule.target_idx].str());

                    // 查找对应的实体 ID
                    auto source_it = name_to_id.find(source_name);
                    auto target_it = name_to_id.find(target_name);

                    if (source_it != name_to_id.end() && target_it != name_to_id.end()) {
                        KGRelation rel;
                        rel.id = generate_relation_id(source_it->second, target_it->second, rule.rel_type);
                        rel.source_id = source_it->second;
                        rel.target_id = target_it->second;
                        rel.type = rule.rel_type;
                        rel.source_chunk_id = chunk_id;

                        relations.push_back(rel);
                    }
                }

                ++it;
            }
        } catch (const std::regex_error& e) {
            RAG_WARN("Regex error: " + std::string(e.what()));
        }
    }

    return relations;
}

std::string RuleBasedExtractor::clean_entity_name(const std::string& name) {
    std::string result = name;

    // 移除前后空白
    result = trim(result);

    // 移除常见前缀
    static const char* prefixes[] = {"the ", "The ", "a ", "A ", "an ", "An "};
    for (const auto& prefix : prefixes) {
        if (result.compare(0, strlen(prefix), prefix) == 0) {
            result = result.substr(strlen(prefix));
        }
    }

    return trim(result);
}

std::vector<KGEntity> RuleBasedExtractor::deduplicate_entities(
    std::vector<KGEntity>& entities) {

    std::unordered_map<std::string, KGEntity> unique;
    std::unordered_set<std::string> seen_names;

    // 按置信度排序后去重 (保留高置信度的)
    std::sort(entities.begin(), entities.end(),
              [](const KGEntity& a, const KGEntity& b) {
                  return a.confidence > b.confidence;
              });

    for (auto& entity : entities) {
        std::string key = to_lower(entity.name);

        if (seen_names.find(key) == seen_names.end()) {
            seen_names.insert(key);
            unique[entity.id] = entity;
        }
    }

    std::vector<KGEntity> result;
    result.reserve(unique.size());
    for (auto& [id, entity] : unique) {
        result.push_back(entity);
    }

    return result;
}

std::string RuleBasedExtractor::generate_entity_id(const std::string& name, EntityType type) {
    std::hash<std::string> hasher;
    size_t hash = hasher(name);
    return "e_" + std::to_string(++entity_counter_) + "_" + std::to_string(hash % 100000);
}

std::string RuleBasedExtractor::generate_relation_id(
    const std::string& source,
    const std::string& target,
    const std::string& type) {

    return "r_" + std::to_string(++relation_counter_) + "_" +
           source.substr(0, 8) + "_" + target.substr(0, 8);
}

void RuleBasedExtractor::add_pattern(EntityType type, const std::string& pattern) {
    entity_patterns_.push_back({type, pattern});
}

void RuleBasedExtractor::add_relation_pattern(
    const std::string& pattern,
    const std::string& rel_type) {

    relation_patterns_.push_back({pattern, rel_type, 1, 2});
}

void RuleBasedExtractor::enable_entity_type(EntityType type, bool enable) {
    enabled_types_[type] = enable;
}

// ========== LLMBasedExtractor 实现 ==========

LLMBasedExtractor::LLMBasedExtractor(void* llm_service)
    : llm_service_(llm_service) {

    // 默认提示词模板
    prompt_template_ = R"(
请从以下文本中提取实体和关系，以 JSON 格式输出。

文本：
{text}

要求：
1. 提取所有实体，包括人物、组织、地点、技术名词等
2. 提取实体之间的关系
3. 使用以下 JSON 格式：

{
  "entities": [
    {"name": "实体名称", "type": "PERSON|ORGANIZATION|LOCATION|TECHNOLOGY|..."}
  ],
  "relations": [
    {"source": "源实体", "target": "目标实体", "type": "关系类型"}
  ]
}
)";
}

ExtractionResult LLMBasedExtractor::extract(
    const std::string& text,
    const std::string& chunk_id) {

    ExtractionResult result;
    result.source_chunk_id = chunk_id;

    auto start = std::chrono::steady_clock::now();

    if (llm_service_ == nullptr) {
        RAG_WARN("LLM service not set, falling back to rule-based extraction");
        RuleBasedExtractor fallback;
        return fallback.extract(text, chunk_id);
    }

    // 1. 生成提示词
    std::string prompt = build_prompt(text);

    // 2. 调用 LLM (这里简化处理)
    RAG_WARN("LLM extraction not fully implemented, using fallback");
    RuleBasedExtractor fallback;
    auto fallback_result = fallback.extract(text, chunk_id);

    result.entities = fallback_result.entities;
    result.relations = fallback_result.relations;

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

std::vector<ExtractionResult> LLMBasedExtractor::extract_batch(
    const std::vector<std::pair<std::string, std::string>>& texts_and_ids) {

    std::vector<ExtractionResult> results;
    results.reserve(texts_and_ids.size());

    for (const auto& [text, chunk_id] : texts_and_ids) {
        results.push_back(extract(text, chunk_id));
    }

    return results;
}

std::string LLMBasedExtractor::build_prompt(const std::string& text) {
    std::string prompt = prompt_template_;
    size_t pos = prompt.find("{text}");
    if (pos != std::string::npos) {
        prompt.replace(pos, 6, text);
    }
    return prompt;
}

void LLMBasedExtractor::set_llm_service(void* llm_service) {
    llm_service_ = llm_service;
}

void LLMBasedExtractor::set_prompt_template(const std::string& template_str) {
    prompt_template_ = template_str;
}

// ========== 工厂函数 ==========

std::unique_ptr<EntityExtractor> create_entity_extractor(const std::string& type) {
    if (type == "llm" || type == "llm-based") {
        return std::make_unique<LLMBasedExtractor>();
    }
    return std::make_unique<RuleBasedExtractor>();
}

}  // namespace rag
