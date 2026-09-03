/**
 * @file graph_support.h
 * @brief Graph RAG 支持 - 集成知识图谱到 RAG 引擎
 */
#pragma once

#include "rag/knowledge_graph.h"
#include "rag/entity_extractor.h"
#include "rag/graph_retriever.h"
#include "rag/retriever.h"
#include <string>
#include <vector>
#include <memory>

// 前向声明
namespace rag { class Database; class ChunkRepository; }

namespace rag {

// ========== Graph 构建配置 ==========

// 使用 Config.h 中的 GraphConfig
// 保留别名以保持 API 兼容性
using GraphBuildConfig = GraphConfig;

// ========== Graph 构建结果 ==========

/**
 * @brief 图谱构建结果
 */
struct GraphBuildResult {
    bool success = false;
    int entities_extracted = 0;
    int relations_extracted = 0;
    int chunks_processed = 0;
    int64_t duration_ms = 0;
    std::string error_message;
};

// ========== Graph 引擎扩展接口 ==========

/**
 * @brief Graph 引擎扩展接口
 *
 * 提供知识图谱相关的功能
 */
class GraphEngineExtension {
public:
    virtual ~GraphEngineExtension() = default;

    /// 构建知识图谱 (从已索引的 chunks)
    virtual GraphBuildResult build_knowledge_graph() = 0;

    /// 增量更新图谱
    virtual GraphBuildResult update_knowledge_graph(
        const std::vector<std::string>& chunk_ids) = 0;

    /// Graph 检索
    virtual GraphRetrievalResult graph_retrieve(
        const std::string& query,
        int top_k = 10) = 0;

    /// 获取知识图谱
    virtual KnowledgeGraph* knowledge_graph() = 0;

    /// 保存图谱
    virtual void save_knowledge_graph() = 0;

    /// 加载图谱
    virtual void load_knowledge_graph() = 0;

    /// 获取图谱统计
    virtual IndexStatus get_graph_status() const = 0;

    /// 检查是否启用 Graph 模式
    virtual bool is_graph_enabled() const = 0;

    /// 设置文本检索器 (用于混合检索)
    virtual void set_text_retriever(std::shared_ptr<Retriever> retriever) = 0;
};

/// 创建 Graph 扩展
std::unique_ptr<GraphEngineExtension> create_graph_extension(
    std::shared_ptr<Database> db,
    const GraphConfig& config,
    std::shared_ptr<KnowledgeGraph> kg = nullptr);

}  // namespace rag
