/**
 * @file tool.cpp
 * @brief Agent Tool 系统实现
 */

#include "rag/modular/agent/tool.h"
#include "rag/error.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

namespace rag::modular::agent {

// ========== VectorSearchTool 实现 ==========

VectorSearchTool::VectorSearchTool(std::shared_ptr<rag::Retriever> retriever, int top_k)
    : retriever_(std::move(retriever)), top_k_(top_k) {
}

std::string VectorSearchTool::description() const {
    return "使用向量检索从知识库中搜索与查询最相关的文本块。输入: query (搜索查询字符串), top_k (返回结果数量，默认5)。输出: JSON格式的检索结果列表，包含文本块内容、分数和来源信息。";
}

std::string VectorSearchTool::parameters_json_schema() const {
    return R"({
        "type": "object",
        "properties": {
            "query": {
                "type": "string",
                "description": "搜索查询字符串"
            },
            "top_k": {
                "type": "integer",
                "description": "返回结果数量",
                "default": 5
            }
        },
        "required": ["query"]
    })";
}

ToolResult VectorSearchTool::execute(const std::string& parameters) {
    auto start_time = std::chrono::steady_clock::now();
    ToolResult result;

    try {
        json params = json::parse(parameters);

        if (!params.contains("query")) {
            result.error = "Missing required parameter: query";
            return result;
        }

        std::string query = params["query"];
        int top_k = params.value("top_k", top_k_);

        auto results = retriever_->retrieve(query, top_k);

        json output;
        output["results"] = json::array();
        for (const auto& r : results) {
            output["results"].push_back({
                {"chunk_id", r.chunk.id},
                {"content", r.chunk.content},
                {"document_id", r.chunk.document_id},
                {"score", r.score},
                {"source", r.source},
                {"rank", r.rank}
            });
        }
        output["total"] = results.size();

        result.success = true;
        result.output = output.dump(2);

    } catch (const json::parse_error& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    } catch (const std::exception& e) {
        result.error = std::string("Execution error: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
}

// ========== BM25SearchTool 实现 ==========

BM25SearchTool::BM25SearchTool(std::shared_ptr<rag::Retriever> retriever, int top_k)
    : retriever_(std::move(retriever)), top_k_(top_k) {
}

std::string BM25SearchTool::description() const {
    return "使用 BM25 全文检索算法从知识库中搜索与查询最相关的文本块。输入: query (搜索查询字符串), top_k (返回结果数量，默认5)。输出: JSON格式的检索结果列表，包含文本块内容、BM25分数和来源信息。";
}

std::string BM25SearchTool::parameters_json_schema() const {
    return R"({
        "type": "object",
        "properties": {
            "query": {
                "type": "string",
                "description": "搜索查询字符串"
            },
            "top_k": {
                "type": "integer",
                "description": "返回结果数量",
                "default": 5
            }
        },
        "required": ["query"]
    })";
}

ToolResult BM25SearchTool::execute(const std::string& parameters) {
    auto start_time = std::chrono::steady_clock::now();
    ToolResult result;

    try {
        json params = json::parse(parameters);

        if (!params.contains("query")) {
            result.error = "Missing required parameter: query";
            return result;
        }

        std::string query = params["query"];
        int top_k = params.value("top_k", top_k_);

        auto results = retriever_->retrieve(query, top_k);

        json output;
        output["results"] = json::array();
        for (const auto& r : results) {
            output["results"].push_back({
                {"chunk_id", r.chunk.id},
                {"content", r.chunk.content},
                {"document_id", r.chunk.document_id},
                {"bm25_score", r.bm25_score},
                {"source", r.source},
                {"rank", r.rank}
            });
        }
        output["total"] = results.size();

        result.success = true;
        result.output = output.dump(2);

    } catch (const json::parse_error& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    } catch (const std::exception& e) {
        result.error = std::string("Execution error: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
}

// ========== GraphSearchTool 实现 ==========

GraphSearchTool::GraphSearchTool(std::shared_ptr<rag::GraphRetriever> graph_retriever, int top_k)
    : graph_retriever_(std::move(graph_retriever)), top_k_(top_k) {
}

std::string GraphSearchTool::description() const {
    return "使用知识图谱检索相关实体和关系。输入: query (搜索查询字符串), top_k (返回结果数量，默认5)。输出: JSON格式的检索结果，包含匹配的实体、关系和关联的文本块。";
}

std::string GraphSearchTool::parameters_json_schema() const {
    return R"({
        "type": "object",
        "properties": {
            "query": {
                "type": "string",
                "description": "搜索查询字符串"
            },
            "top_k": {
                "type": "integer",
                "description": "返回结果数量",
                "default": 5
            }
        },
        "required": ["query"]
    })";
}

ToolResult GraphSearchTool::execute(const std::string& parameters) {
    auto start_time = std::chrono::steady_clock::now();
    ToolResult result;

    try {
        json params = json::parse(parameters);

        if (!params.contains("query")) {
            result.error = "Missing required parameter: query";
            return result;
        }

        std::string query = params["query"];
        int top_k = params.value("top_k", top_k_);

        rag::RetrievalConfig retrieval_config;
        retrieval_config.top_k = top_k;
        auto graph_result = graph_retriever_->retrieve(query, retrieval_config);

        json output;
        output["matched_entities"] = json::array();
        for (const auto& entity : graph_result.matched_entities) {
            output["matched_entities"].push_back({
                {"id", entity.id},
                {"name", entity.name},
                {"type", entity.type}
            });
        }

        output["chunks"] = json::array();
        for (const auto& chunk : graph_result.chunks) {
            output["chunks"].push_back({
                {"chunk_id", chunk.chunk.id},
                {"content", chunk.chunk.content},
                {"score", chunk.score},
                {"source", chunk.source}
            });
        }

        output["processing_time_ms"] = graph_result.processing_time_ms;
        output["score"] = graph_result.score;

        result.success = true;
        result.output = output.dump(2);

    } catch (const json::parse_error& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    } catch (const std::exception& e) {
        result.error = std::string("Execution error: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
}

// ========== LLMGenerateTool 实现 ==========

LLMGenerateTool::LLMGenerateTool(std::shared_ptr<rag::LLMService> llm_service)
    : llm_service_(std::move(llm_service)) {
}

std::string LLMGenerateTool::description() const {
    return "使用 LLM 生成回答。输入: prompt (生成提示词), max_tokens (最大生成长度，默认1024), temperature (温度参数，默认0.7)。输出: JSON格式的生成结果，包含生成的文本、token数量和耗时。";
}

std::string LLMGenerateTool::parameters_json_schema() const {
    return R"({
        "type": "object",
        "properties": {
            "prompt": {
                "type": "string",
                "description": "生成提示词"
            },
            "max_tokens": {
                "type": "integer",
                "description": "最大生成长度",
                "default": 1024
            },
            "temperature": {
                "type": "number",
                "description": "温度参数",
                "default": 0.7
            }
        },
        "required": ["prompt"]
    })";
}

ToolResult LLMGenerateTool::execute(const std::string& parameters) {
    auto start_time = std::chrono::steady_clock::now();
    ToolResult result;

    try {
        json params = json::parse(parameters);

        if (!params.contains("prompt")) {
            result.error = "Missing required parameter: prompt";
            return result;
        }

        std::string prompt = params["prompt"];
        int max_tokens = params.value("max_tokens", 1024);
        float temperature = params.value("temperature", 0.7f);

        rag::GenerateOptions options;
        options.max_tokens = max_tokens;
        options.temperature = temperature;

        auto generate_result = llm_service_->generate(prompt, options);

        json output;
        output["text"] = generate_result.text;
        output["tokens_generated"] = generate_result.tokens_generated;
        output["finish_reason"] = generate_result.finish_reason;
        output["duration_ms"] = generate_result.duration_ms;

        result.success = true;
        result.output = output.dump(2);

    } catch (const json::parse_error& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    } catch (const std::exception& e) {
        result.error = std::string("Execution error: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
}

// ========== ToolRegistry 实现 ==========

void ToolRegistry::register_tool(std::shared_ptr<Tool> tool) {
    if (tool) {
        tools_[tool->name()] = std::move(tool);
    }
}

std::shared_ptr<Tool> ToolRegistry::get_tool(const std::string& name) {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> ToolRegistry::list_tools() const {
    std::vector<std::string> names;
    names.reserve(tools_.size());
    for (const auto& pair : tools_) {
        names.push_back(pair.first);
    }
    return names;
}

bool ToolRegistry::has_tool(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

void ToolRegistry::unregister_tool(const std::string& name) {
    tools_.erase(name);
}

void ToolRegistry::clear() {
    tools_.clear();
}

}  // namespace rag::modular::agent
