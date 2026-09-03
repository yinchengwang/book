/**
 * @file llm_service.cpp
 * @brief LLM 服务实现
 */

#include "rag/llm_service.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>
#include <thread>

namespace rag {

// ========== SimpleLLMService 实现 ==========

SimpleLLMService::SimpleLLMService() = default;

SimpleLLMService::~SimpleLLMService() {
    unload();
}

void SimpleLLMService::load(const std::string& model_path, const LLMConfig& config) {
    if (loaded_) {
        unload();
    }

    model_path_ = model_path;
    config_ = config;

    // 模拟加载
    RAG_INFO("Loading LLM model: " + model_path);
    RAG_INFO("Model type: " + config.model_type);
    RAG_INFO("Context window: " + std::to_string(config.n_ctx));

    // 简单验证
    if (model_path.empty()) {
        throw LLMException(ErrorCodes::MODEL_NOT_FOUND,
                          "Model path is empty");
    }

    // 模拟加载延迟
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    loaded_ = true;
    RAG_INFO("LLM model loaded successfully");
}

void SimpleLLMService::unload() {
    if (!loaded_) {
        return;
    }

    RAG_INFO("Unloading LLM model...");
    loaded_ = false;
    model_path_.clear();
}

GenerateResult SimpleLLMService::generate(const std::string& prompt,
                                          const GenerateOptions& options) {
    auto start = std::chrono::steady_clock::now();

    if (!loaded_) {
        throw LLMException(ErrorCodes::LLM_NOT_AVAILABLE,
                          "LLM model not loaded");
    }

    GenerateResult result;
    result.text = simulate_generate(prompt);
    result.finished = true;
    result.tokens_generated = static_cast<int>(result.text.size()) / 4;  // 估算
    result.finish_reason = "stop";

    auto end = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    return result;
}

void SimpleLLMService::generate_stream(const std::string& prompt,
                                       const GenerateOptions& options,
                                       StreamCallback callback) {
    if (!loaded_) {
        throw LLMException(ErrorCodes::LLM_NOT_AVAILABLE,
                          "LLM model not loaded");
    }

    std::string full_response = simulate_generate(prompt);

    // 模拟流式输出
    std::istringstream iss(full_response);
    std::string word;
    while (std::getline(iss, word, ' ')) {
        if (!word.empty()) {
            word += " ";
            callback(word, false);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    callback("", true);  // 完成
}

std::vector<GenerateResult> SimpleLLMService::generate_batch(
    const std::vector<std::string>& prompts,
    const GenerateOptions& options) {

    std::vector<GenerateResult> results;
    results.reserve(prompts.size());

    for (const auto& prompt : prompts) {
        results.push_back(generate(prompt, options));
    }

    return results;
}

std::string SimpleLLMService::simulate_generate(const std::string& prompt) {
    // 简单的模拟生成
    // 生产环境应使用真正的 LLM 推理

    std::ostringstream oss;

    // 检测关键词并生成相关回复
    if (prompt.find("RAG") != std::string::npos || prompt.find("检索") != std::string::npos) {
        oss << "RAG（检索增强生成）是一种结合检索系统和语言模型的技术。\n\n";
        oss << "它的工作流程包括：\n";
        oss << "1. 将文档分割成小块（Chunking）\n";
        oss << "2. 为每个块生成向量嵌入（Embedding）\n";
        oss << "3. 将向量存储在向量数据库中\n";
        oss << "4. 查询时，先检索相关块\n";
        oss << "5. 将检索结果作为上下文传给 LLM\n";
        oss << "6. LLM 基于上下文生成回答\n\n";
        oss << "这种方法可以提高回答的准确性和可追溯性。";
    } else if (prompt.find("索引") != std::string::npos || prompt.find("index") != std::string::npos) {
        oss << "构建索引是 RAG 系统的核心步骤：\n\n";
        oss << "1. 文档解析：将各种格式的文档转换为纯文本\n";
        oss << "2. 文本分块：按策略将长文本分割成小块\n";
        oss << "3. 向量编码：使用 Embedding 模型将文本转为向量\n";
        oss << "4. 索引存储：将向量存入 HNSW 等向量索引\n";
        oss << "5. 元数据存储：在 SQLite 中保存文档和块的元数据\n\n";
        oss << "索引构建完成后，系统就可以进行高效的相似性检索。";
    } else if (prompt.find("HNSW") != std::string::npos) {
        oss << "HNSW（Hierarchical Navigable Small World）是一种高效的近似最近邻搜索算法：\n\n";
        oss << "特点：\n";
        oss << "- 构建多层图结构，每层是不同精度的图\n";
        oss << "- 搜索时从上到下逐层精炼\n";
        oss << "- 查询速度快，精度可调\n";
        oss << "- 内存占用相对较高\n\n";
        oss << "关键参数：\n";
        oss << "- M: 每个节点的连接数\n";
        oss << "- efConstruction: 构建时的搜索范围\n";
        oss << "- efSearch: 查询时的搜索范围";
    } else if (prompt.find("BM25") != std::string::npos) {
        oss << "BM25（Best Matching 25）是一种经典的文本检索算法：\n\n";
        oss << "核心思想：\n";
        oss << "- 基于词频和逆文档频率计算相关性\n";
        oss << "- 考虑文档长度对相关性的影响\n\n";
        oss << "公式：\n";
        oss << "score = IDF * (tf * (k1 + 1)) / (tf + k1 * (1 - b + b * |d| / avg_d))\n\n";
        oss << "参数：\n";
        oss << "- k1: 词频饱和度（通常 1.2-2.0）\n";
        oss << "- b: 文档长度归一化（通常 0.75）";
    } else if (prompt.find("代码") != std::string::npos || prompt.find("code") != std::string::npos) {
        oss << "这是一个代码相关的查询。以下是一些代码示例：\n\n";
        oss << "```cpp\n";
        oss << "// RAG 引擎初始化示例\n";
        oss << "RAGEngine engine;\n";
        oss << "engine.init(config);\n\n";
        oss << "// 构建索引\n";
        oss << "auto result = engine.build_index();\n\n";
        oss << "// 查询\n";
        oss << "auto answer = engine.query(\"问题\");\n";
        oss << "```\n\n";
        oss << "代码展示了 RAG 系统的基础用法。";
    } else {
        // 默认回复
        oss << "基于您的问题，我在文档库中找到以下相关信息：\n\n";
        oss << "根据 RAG 系统的设计，这涉及到检索增强生成的核心概念。\n\n";
        oss << "关键要点：\n";
        oss << "1. 系统首先对文档进行解析和分块\n";
        oss << "2. 文本块被编码为向量表示\n";
        oss << "3. 查询时，系统检索最相关的块\n";
        oss << "4. 相关块作为上下文传给语言模型\n";
        oss << "5. 模型基于上下文生成准确的回答\n\n";
        oss << "这种方法可以有效利用外部知识库，提升回答质量。";
    }

    return oss.str();
}

// ========== 工厂函数 ==========

std::unique_ptr<LLMService> create_llm_service() {
    return std::make_unique<SimpleLLMService>();
}

std::unique_ptr<LLMService> create_llm_service(const std::string& type) {
    // 未来可以添加更多类型
    return std::make_unique<SimpleLLMService>();
}

}  // namespace rag
