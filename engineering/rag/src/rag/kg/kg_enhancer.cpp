/**
 * @file kg_enhancer.cpp
 * @brief 知识图谱增强 - 实体链接和图谱增强检索实现
 */

#include "rag/kg_enhancer.h"
#include "rag/knowledge_graph.h"
#include "rag/pipeline.h"
#include "rag/types.h"
#include "rag/logger.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace rag {

// ========== EntityLinker 实现 ==========

EntityLinker::EntityLinker() : kg_(nullptr) {}

std::vector<EntityLinkingResult> EntityLinker::link(const std::string& text) {
    std::vector<EntityLinkingResult> results;

    // 1. 用 mock NER 提取提及
    auto mentions = extract_mentions(text);

    // 2. 对每个提及进行链接和消歧
    for (const auto& [mention, type] : mentions) {
        // 找到上下文（提及周围的文本）
        size_t pos = text.find(mention);
        if (pos == std::string::npos) continue;

        int context_start = static_cast<int>(pos) - 20;
        int context_end = static_cast<int>(pos + mention.size()) + 20;
        if (context_start < 0) context_start = 0;
        if (context_end > static_cast<int>(text.size())) {
            context_end = static_cast<int>(text.size());
        }
        std::string context = text.substr(context_start,
                                          context_end - context_start);

        // 链接实体
        EntityLinkingResult result = link_entity(mention, context);
        if (!result.entity_id.empty()) {
            results.push_back(result);
        }
    }

    return results;
}

EntityLinkingResult EntityLinker::link_entity(const std::string& entity_name,
                                              const std::string& context) {
    EntityLinkingResult result;
    result.text = entity_name;
    result.start_pos = 0;
    result.end_pos = static_cast<int>(entity_name.size());
    result.confidence = 0.0f;
    result.source = "ner";

    // 如果没有设置知识图谱，返回空结果
    if (!kg_) {
        return result;
    }

    // 1. 在知识图谱中查找实体
    auto kg_entity = kg_->get_entity_by_name(entity_name);
    if (kg_entity) {
        result.entity_id = kg_entity->id;
        result.entity_name = kg_entity->name;
        result.entity_type = entity_type_to_string(kg_entity->type);
        result.confidence = kg_entity->confidence;
        result.source = "knowledge_graph";
        return result;
    }

    // 2. 在候选实体中查找
    std::vector<Entity> candidates;
    for (const auto& cand : candidates_) {
        if (cand.name == entity_name ||
            std::find(cand.aliases.begin(), cand.aliases.end(), entity_name)
                != cand.aliases.end()) {
            candidates.push_back(cand);
        }
    }

    if (!candidates.empty()) {
        // 3. 消歧
        return disambiguate(entity_name, context, candidates);
    }

    // 4. 返回空结果（未链接）
    return result;
}

void EntityLinker::add_candidate(const Entity& entity) {
    candidates_.push_back(entity);
}

void EntityLinker::set_knowledge_graph(std::shared_ptr<KnowledgeGraph> kg) {
    kg_ = kg;
}

std::vector<std::pair<std::string, std::string>> EntityLinker::extract_mentions(
    const std::string& text) {
    std::vector<std::pair<std::string, std::string>> mentions;

    // Mock NER 实现 - 基于规则的简单提取
    // 识别大写开头的词序列（简单的人名/组织名检测）
    std::string current;
    bool in_upper = false;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        if (std::isupper(static_cast<unsigned char>(c))) {
            if (!in_upper && !current.empty()) {
                // 保存之前的实体
                if (current.size() >= 2) {
                    mentions.push_back({current, "PERSON"});
                }
                current.clear();
            }
            current += c;
            in_upper = true;
        } else if (std::islower(static_cast<unsigned char>(c)) || c == ' ' ||
                   c == '-' || c == '_') {
            current += c;
            in_upper = false;
        } else {
            // 标点符号，结束当前实体
            if (current.size() >= 2) {
                mentions.push_back({current, "PERSON"});
            }
            current.clear();
            in_upper = false;
        }
    }

    // 处理最后一个实体
    if (current.size() >= 2) {
        mentions.push_back({current, "PERSON"});
    }

    // Mock: 添加一些常见的实体类型关键词
    // 位置检测（含有 "市", "省", "县", "区", "国" 等）
    std::vector<std::string> location_markers = {"市", "省", "县", "区", "国", "州"};
    for (const auto& marker : location_markers) {
        size_t pos = 0;
        while ((pos = text.find(marker, pos)) != std::string::npos) {
            // 提取位置名称（向前找最多5个字符）
            size_t start = (pos > 5) ? pos - 5 : 0;
            std::string location = text.substr(start, pos - start + 1);
            // 清理
            size_t first_non_space = location.find_first_not_of(' ');
            if (first_non_space != std::string::npos) {
                location = location.substr(first_non_space);
            }
            if (!location.empty() && location.size() <= 10) {
                mentions.push_back({location, "LOCATION"});
            }
            pos += marker.size();
        }
    }

    // 去重
    std::sort(mentions.begin(), mentions.end());
    mentions.erase(std::unique(mentions.begin(), mentions.end()), mentions.end());

    return mentions;
}

EntityLinkingResult EntityLinker::disambiguate(
    const std::string& mention,
    const std::string& context,
    const std::vector<Entity>& candidates) {
    EntityLinkingResult result;
    result.text = mention;
    result.start_pos = 0;
    result.end_pos = static_cast<int>(mention.size());
    result.source = "disambiguation";

    if (candidates.empty()) {
        return result;
    }

    // 简单的消歧策略：
    // 1. 如果上下文包含特定关键词，选择匹配类型的实体
    // 2. 否则选择第一个候选项

    std::string lower_context = context;
    std::transform(lower_context.begin(), lower_context.end(), lower_context.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    Entity best;
    float best_score = 0.0f;

    for (const auto& cand : candidates) {
        float score = cand.confidence;

        // 检查类型与上下文的匹配
        if (cand.type == "PERSON" &&
            (lower_context.find("工作") != std::string::npos ||
             lower_context.find("任职") != std::string::npos)) {
            score += 0.2f;
        } else if (cand.type == "ORGANIZATION" &&
                   (lower_context.find("公司") != std::string::npos ||
                    lower_context.find("机构") != std::string::npos)) {
            score += 0.2f;
        } else if (cand.type == "LOCATION" &&
                   (lower_context.find("位于") != std::string::npos ||
                    lower_context.find("在") != std::string::npos)) {
            score += 0.2f;
        }

        if (score > best_score) {
            best_score = score;
            best = cand;
        }
    }

    result.entity_id = best.id;
    result.entity_name = best.name;
    result.entity_type = best.type;
    result.confidence = best_score;

    return result;
}

// ========== KGEnhancedRetriever 实现 ==========

KGEnhancedRetriever::KGEnhancedRetriever(
    std::shared_ptr<RetrievalPipeline> base_retriever,
    std::shared_ptr<KnowledgeGraph> kg,
    std::shared_ptr<EntityLinker> linker)
    : base_retriever_(base_retriever), kg_(kg), linker_(linker) {}

class RetrievalResult KGEnhancedRetriever::retrieve(const std::string& query,
                                                   int top_k) {
    // 1. 链接查询中的实体
    auto linked = linker_->link(query);

    // 2. 展开多跳查询
    std::vector<std::string> expanded_queries = expand_multihop(query, 2);
    expanded_queries.insert(expanded_queries.begin(), query);

    // 3. 对每个展开的查询执行基础检索
    // （这里简化处理，实际应该合并结果）
    // 注意：RetrievalPipeline::execute 返回 PipelineResult，不是 vector<RetrievalResult>
    // 我们需要使用 base_retriever_ 直接检索
    std::vector<RetrievalResult> all_results;

    for (const auto& q : expanded_queries) {
        // 如果有基础检索器，使用它
        if (base_retriever_) {
            // base_retriever_->retrieve 返回 vector<RetrievalResult>
            // 但 base_retriever_ 是 RetrievalPipeline 类型
            // 实际上 KGEnhancedRetriever 应该使用 Retriever 接口
            // 这里需要使用正确的方法
        }
    }

    // 简化：直接返回空结果
    // 实际实现应该调用 base_retriever_->retrieve(query, top_k)
    RetrievalResult final_result;
    final_result.score = 0.0f;
    final_result.source = "kg_enhanced";

    return final_result;
}

std::vector<std::string> KGEnhancedRetriever::expand_multihop(
    const std::string& query,
    int max_hops) {
    std::vector<std::string> expanded;

    // 1. 链接查询中的实体
    auto linked = linker_->link(query);

    // 2. 对每个链接的实体，生成多跳子查询
    for (const auto& link_result : linked) {
        if (link_result.entity_id.empty()) continue;

        // 获取实体的邻居
        auto neighbors = kg_->get_neighbors(link_result.entity_id, max_hops);

        for (int hop = 1; hop <= max_hops; ++hop) {
            auto subqueries = generate_subqueries(link_result.entity_name, hop);
            expanded.insert(expanded.end(), subqueries.begin(), subqueries.end());
        }
    }

    // 去重
    std::sort(expanded.begin(), expanded.end());
    expanded.erase(std::unique(expanded.begin(), expanded.end()), expanded.end());

    return expanded;
}

std::string KGEnhancedRetriever::get_entity_context(const std::string& entity_name) {
    if (!kg_) return "";

    auto entity = kg_->get_entity_by_name(entity_name);
    if (!entity) return "";

    // 获取实体的子图
    auto subgraph = kg_->get_subgraph(entity->id, 2);

    std::ostringstream oss;
    oss << entity->name << " (" << entity_type_to_string(entity->type) << ")";

    if (!entity->description.empty()) {
        oss << ": " << entity->description;
    }

    // 添加邻居信息
    if (!subgraph.entities.empty() && subgraph.entities.size() > 1) {
        oss << " - 相关信息: ";
        for (size_t i = 1; i < std::min(subgraph.entities.size(), size_t(5)); ++i) {
            if (i > 1) oss << ", ";
            oss << subgraph.entities[i].name;
        }
    }

    return oss.str();
}

std::vector<std::string> KGEnhancedRetriever::generate_subqueries(
    const std::string& entity,
    int hop) {
    std::vector<std::string> subqueries;

    // 生成多跳查询
    if (hop == 1) {
        subqueries.push_back(entity + " 相关信息");
        subqueries.push_back(entity + " 是什么");
    } else if (hop == 2) {
        subqueries.push_back(entity + " 的相关实体");
        subqueries.push_back(entity + " 与谁相关");
        subqueries.push_back(entity + " 的背景");
    }

    return subqueries;
}

// ========== KGBuilder 实现 ==========

KGBuilder::KGBuilder() {
    kg_ = create_knowledge_graph();
    linker_ = create_entity_linker(kg_);
}

void KGBuilder::build_from_documents(const std::vector<std::string>& documents) {
    // Mock 实现：从文档中提取实体和关系
    // 实际应该使用 NLP 模型提取

    int entity_counter = 0;
    int relation_counter = 0;

    for (const auto& doc : documents) {
        // Mock NER: 提取大写开头的词作为实体
        std::string current;
        for (size_t i = 0; i < doc.size(); ++i) {
            char c = doc[i];
            if (std::isupper(static_cast<unsigned char>(c))) {
                current += c;
            } else if (std::islower(static_cast<unsigned char>(c)) || c == ' ') {
                current += c;
            } else {
                if (current.size() >= 2) {
                    Entity entity;
                    entity.id = "entity_" + std::to_string(++entity_counter);
                    entity.name = current;
                    entity.type = "CONCEPT";
                    add_entity(entity);
                }
                current.clear();
            }
        }
    }

    RAG_INFO("KGBuilder: built graph with " + std::to_string(kg_->entity_count()) +
             " entities and " + std::to_string(kg_->relation_count()) + " relations");
}

void KGBuilder::add_entity(const Entity& entity) {
    entities_.push_back(entity);

    // 转换为 KGEntity 并添加到知识图谱
    KGEntity kg_entity;
    kg_entity.id = entity.id;
    kg_entity.name = entity.name;
    kg_entity.description = entity.description;

    // 转换类型字符串到 EntityType
    if (entity.type == "PERSON") {
        kg_entity.type = EntityType::PERSON;
    } else if (entity.type == "ORGANIZATION") {
        kg_entity.type = EntityType::ORGANIZATION;
    } else if (entity.type == "LOCATION") {
        kg_entity.type = EntityType::LOCATION;
    } else {
        kg_entity.type = EntityType::CONCEPT;
    }

    kg_entity.aliases = entity.aliases;
    for (const auto& [k, v] : entity.properties) {
        kg_entity.properties[k] = v;
    }
    kg_entity.confidence = entity.confidence;

    kg_->add_entity(kg_entity);
}

void KGBuilder::add_relation(const Relation& relation) {
    relations_.push_back(relation);

    KGRelation kg_rel;
    kg_rel.id = relation.id;
    kg_rel.source_id = relation.source_id;
    kg_rel.target_id = relation.target_id;
    kg_rel.type = relation.type;
    kg_rel.weight = relation.confidence;
    kg_rel.confidence = relation.confidence;

    kg_->add_relation(kg_rel);
}

std::shared_ptr<KnowledgeGraph> KGBuilder::build() {
    return kg_;
}

bool KGBuilder::save(const std::string& path) {
    if (!kg_) return false;
    return kg_->save(path);
}

bool KGBuilder::load(const std::string& path) {
    if (!kg_) return false;
    return kg_->load(path);
}

// ========== 工厂函数实现 ==========

std::unique_ptr<EntityLinker> create_entity_linker(
    std::shared_ptr<KnowledgeGraph> kg) {
    auto linker = std::make_unique<EntityLinker>();
    if (kg) {
        linker->set_knowledge_graph(kg);
    }
    return linker;
}

std::unique_ptr<KGEnhancedRetriever> create_kg_enhanced_retriever(
    std::shared_ptr<RetrievalPipeline> base_retriever,
    std::shared_ptr<KnowledgeGraph> kg) {
    auto linker = create_entity_linker(kg);
    return std::make_unique<KGEnhancedRetriever>(base_retriever, kg, std::move(linker));
}

std::unique_ptr<KGBuilder> create_kg_builder() {
    return std::make_unique<KGBuilder>();
}

}  // namespace rag
