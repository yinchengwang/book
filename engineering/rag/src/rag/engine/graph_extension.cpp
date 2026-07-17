/**
 * @file graph_extension.cpp
 * @brief Graph 引擎扩展实现
 */

#include "rag/graph_support.h"
#include "rag/knowledge_graph.h"
#include "rag/logger.h"
#include "rag/engine.h"
#include "rag/database.h"
#include <fstream>
#include <chrono>
#include <algorithm>

namespace rag {

// ========== GraphEngineExtension 实现 ==========

class GraphEngineExtensionImpl : public GraphEngineExtension {
public:
    GraphEngineExtensionImpl(
        std::shared_ptr<Database> db,
        const GraphConfig& config,
        std::shared_ptr<KnowledgeGraph> kg = nullptr);

    GraphBuildResult build_knowledge_graph() override;

    GraphBuildResult update_knowledge_graph(
        const std::vector<std::string>& chunk_ids) override;

    GraphRetrievalResult graph_retrieve(
        const std::string& query,
        int top_k = 10) override;

    KnowledgeGraph* knowledge_graph() override { return kg_.get(); }

    void save_knowledge_graph() override;

    void load_knowledge_graph() override;

    IndexStatus get_graph_status() const override;

    bool is_graph_enabled() const override { return config_.enabled; }

    void set_text_retriever(std::shared_ptr<Retriever> retriever) override {
        if (graph_retriever_) {
            graph_retriever_->set_text_retriever(retriever.get());
        }
    }

private:
    /// 处理单个 chunk
    ExtractionResult process_chunk(const Chunk& chunk);

    /// 合并实体到图谱
    void merge_entities(const std::vector<KGEntity>& entities);

    /// 合并关系到图谱
    void merge_relations(const std::vector<KGRelation>& relations);

    /// 实体去重和合并
    KGEntity merge_or_create_entity(const KGEntity& entity);

    GraphConfig config_;
    std::shared_ptr<Database> db_;
    std::shared_ptr<KnowledgeGraph> kg_;
    std::shared_ptr<EntityExtractor> extractor_;
    std::shared_ptr<GraphRetriever> graph_retriever_;
    std::shared_ptr<RetrievalConfig> retrieval_config_;
    std::filesystem::path kg_storage_path_;
};

// ========== 实现 ==========

GraphEngineExtensionImpl::GraphEngineExtensionImpl(
    std::shared_ptr<Database> db,
    const GraphConfig& config,
    std::shared_ptr<KnowledgeGraph> kg)
    : config_(config),
      db_(db),
      kg_(kg),
      kg_storage_path_(config.kg_storage_path) {

    if (config.enabled) {
        // 创建知识图谱
        if (!kg) {
            kg_ = create_knowledge_graph();
        } else {
            kg_ = kg;
        }

        // 创建实体提取器
        extractor_ = create_entity_extractor(config.extractor_type);

        // 创建图检索器
        GraphRetrievalConfig grc;
        grc.max_hops = config.max_hops;
        grc.include_subgraph = true;
        grc.include_text_chunks = true;
        graph_retriever_ = create_graph_retriever("hybrid", grc);
        graph_retriever_->set_knowledge_graph(kg_.get());

        // 创建检索配置
        retrieval_config_ = std::make_shared<RetrievalConfig>();
        retrieval_config_->top_k = 10;

        RAG_INFO("Graph support initialized with extractor: " + config.extractor_type);
    }
}

ExtractionResult GraphEngineExtensionImpl::process_chunk(const Chunk& chunk) {
    return extractor_->extract(chunk.content, chunk.id);
}

KGEntity GraphEngineExtensionImpl::merge_or_create_entity(const KGEntity& entity) {
    // 检查是否已存在
    auto existing = kg_->get_entity_by_name(entity.name);
    if (existing.has_value()) {
        // 合并属性
        KGEntity merged = existing.value();
        if (entity.confidence > merged.confidence) {
            merged.confidence = entity.confidence;
        }
        // 合并别名
        for (const auto& alias : entity.aliases) {
            if (std::find(merged.aliases.begin(), merged.aliases.end(), alias) == merged.aliases.end()) {
                merged.aliases.push_back(alias);
            }
        }
        // 合并属性
        for (const auto& [k, v] : entity.properties) {
            if (merged.properties.find(k) == merged.properties.end()) {
                merged.properties[k] = v;
            }
        }
        kg_->update_entity(merged);
        return merged;
    }

    // 创建新实体
    kg_->add_entity(entity);
    return entity;
}

void GraphEngineExtensionImpl::merge_entities(const std::vector<KGEntity>& entities) {
    for (const auto& entity : entities) {
        if (entity.confidence >= config_.entity_confidence_threshold) {
            merge_or_create_entity(entity);
        }
    }
}

void GraphEngineExtensionImpl::merge_relations(const std::vector<KGRelation>& relations) {
    for (const auto& rel : relations) {
        // 检查源实体和目标实体是否存在
        auto src = kg_->get_entity(rel.source_id);
        auto tgt = kg_->get_entity(rel.target_id);

        if (src.has_value() && tgt.has_value()) {
            // 检查关系是否已存在
            auto existing_rels = kg_->get_relations(rel.source_id);
            bool found = false;
            for (const auto& existing : existing_rels) {
                if (existing.target_id == rel.target_id && existing.type == rel.type) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                kg_->add_relation(rel);
            }
        }
    }
}

GraphBuildResult GraphEngineExtensionImpl::build_knowledge_graph() {
    GraphBuildResult result;

    if (!config_.enabled) {
        result.error_message = "Graph support not enabled";
        return result;
    }

    auto start = std::chrono::steady_clock::now();

    try {
        // 查询所有 chunks
        auto rows = db_->query("SELECT id, content, document_id FROM chunks");
        result.chunks_processed = static_cast<int>(rows.size());

        int entities_count = 0;
        int relations_count = 0;

        for (const auto& row : rows) {
            Chunk chunk;
            auto id_it = row.find("id");
            auto content_it = row.find("content");
            auto doc_id_it = row.find("document_id");

            if (id_it == row.end() || content_it == row.end()) continue;

            chunk.id = id_it->second;
            chunk.content = content_it->second;
            if (doc_id_it != row.end()) chunk.document_id = doc_id_it->second;

            auto extraction = process_chunk(chunk);

            // 合并到图谱
            merge_entities(extraction.entities);
            merge_relations(extraction.relations);

            entities_count += static_cast<int>(extraction.entities.size());
            relations_count += static_cast<int>(extraction.relations.size());
        }

        result.entities_extracted = entities_count;
        result.relations_extracted = relations_count;
        result.success = true;

        RAG_INFO("Built knowledge graph: " + std::to_string(entities_count)
                 + " entities, " + std::to_string(relations_count) + " relations");

    } catch (const std::exception& e) {
        result.error_message = e.what();
        RAG_ERROR("Failed to build knowledge graph: " + std::string(e.what()));
    }

    auto end = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

GraphBuildResult GraphEngineExtensionImpl::update_knowledge_graph(
    const std::vector<std::string>& chunk_ids) {

    GraphBuildResult result;

    if (!config_.enabled) {
        result.error_message = "Graph support not enabled";
        return result;
    }

    auto start = std::chrono::steady_clock::now();

    try {
        int entities_count = 0;
        int relations_count = 0;

        for (const auto& chunk_id : chunk_ids) {
            auto rows = db_->query(
                "SELECT id, content, document_id FROM chunks WHERE id = ?",
                {chunk_id});

            if (!rows.empty()) {
                const auto& row = rows[0];
                Chunk chunk;
                auto id_it = row.find("id");
                auto content_it = row.find("content");
                auto doc_id_it = row.find("document_id");

                if (id_it != row.end() && content_it != row.end()) {
                    chunk.id = id_it->second;
                    chunk.content = content_it->second;
                    if (doc_id_it != row.end()) chunk.document_id = doc_id_it->second;

                    auto extraction = process_chunk(chunk);

                    merge_entities(extraction.entities);
                    merge_relations(extraction.relations);

                    entities_count += static_cast<int>(extraction.entities.size());
                    relations_count += static_cast<int>(extraction.relations.size());
                    result.chunks_processed++;
                }
            }
        }

        result.entities_extracted = entities_count;
        result.relations_extracted = relations_count;
        result.success = true;

        RAG_DEBUG("Updated knowledge graph: " + std::to_string(entities_count)
                  + " entities, " + std::to_string(relations_count) + " relations");

    } catch (const std::exception& e) {
        result.error_message = e.what();
        RAG_ERROR("Failed to update knowledge graph: " + std::string(e.what()));
    }

    auto end = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

GraphRetrievalResult GraphEngineExtensionImpl::graph_retrieve(
    const std::string& query,
    int top_k) {

    if (!config_.enabled || !graph_retriever_) {
        return GraphRetrievalResult();
    }

    retrieval_config_->top_k = top_k;
    return graph_retriever_->retrieve(query, *retrieval_config_);
}

void GraphEngineExtensionImpl::save_knowledge_graph() {
    if (!kg_ || !config_.enabled) {
        return;
    }

    try {
        kg_->save(kg_storage_path_.string());
        RAG_INFO("Saved knowledge graph to " + kg_storage_path_.string());
    } catch (const std::exception& e) {
        RAG_ERROR("Failed to save knowledge graph: " + std::string(e.what()));
    }
}

void GraphEngineExtensionImpl::load_knowledge_graph() {
    if (!kg_ || !config_.enabled) {
        return;
    }

    if (std::filesystem::exists(kg_storage_path_)) {
        try {
            kg_->load(kg_storage_path_.string());
            RAG_INFO("Loaded knowledge graph from " + kg_storage_path_.string());
        } catch (const std::exception& e) {
            RAG_WARN("Failed to load knowledge graph: " + std::string(e.what()));
        }
    }
}

IndexStatus GraphEngineExtensionImpl::get_graph_status() const {
    IndexStatus status;
    status.index_name = "knowledge_graph";

    if (kg_) {
        status.chunk_count = kg_->entity_count();
        status.vector_count = kg_->relation_count();
        status.status = IndexStatus::Status::READY;
    } else {
        status.status = IndexStatus::Status::NOT_EXISTS;
    }

    return status;
}

// ========== 工厂函数 ==========

std::unique_ptr<GraphEngineExtension> create_graph_extension(
    std::shared_ptr<Database> db,
    const GraphConfig& config,
    std::shared_ptr<KnowledgeGraph> kg) {

    return std::make_unique<GraphEngineExtensionImpl>(
        db, config, kg);
}

}  // namespace rag
