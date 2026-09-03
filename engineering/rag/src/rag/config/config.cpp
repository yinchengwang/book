/**
 * @file config.cpp
 * @brief 配置管理实现 - YAML 解析
 */

#include "rag/config.h"
#include "rag/logger.h"
#include <fstream>
#include <sstream>

namespace rag {

// ========== Config 实现 ==========

bool Config::validate() const {
    // 验证数据源
    if (data_sources.empty()) {
        RAG_WARN("No data sources configured");
    }

    // 验证 Embedding 模型
    if (embedding.model_path.empty()) {
        RAG_WARN("Embedding model path not set");
    }

    // 验证 LLM 模型
    if (llm.model_path.empty()) {
        RAG_WARN("LLM model path not set");
    }

    // 验证索引维度
    if (hnsw.dim <= 0) {
        RAG_ERROR("Invalid HNSW dimension");
        return false;
    }

    return true;
}

Config Config::default_config() {
    Config config;

    // 数据源
    config.data_sources.push_back(DataSourceConfig{
        .path = "./docs",
        .patterns = {"*.md", "*.cpp", "*.h", "*.c"},
        .recursive = true
    });

    // Embedding
    config.embedding = {
        .model_path = "./models/bge-base-zh-v1.5",
        .model_type = "bge-base-zh-v1.5",
        .batch_size = 32,
        .max_length = 512,
        .device = "cpu",
        .num_threads = 4
    };

    // LLM
    config.llm = {
        .model_path = "./models/Qwen2.5-7B-Q4_K_M.gguf",
        .model_type = "qwen2.5-7b",
        .n_ctx = 4096,
        .n_threads = 4,
        .max_tokens = 1024,
        .temperature = 0.7f,
        .top_p = 0.9f,
        .stream = false
    };

    // HNSW
    config.hnsw = {
        .dim = 768,
        .max_elements = 1000000,
        .m = 16,
        .ef_construction = 200,
        .ef_search = 100,
        .space = "l2"
    };

    // BM25
    config.bm25 = {
        .k1 = 1.5,
        .b = 0.75f,
        .avg_doc_len = 256
    };

    // 检索
    config.retrieval = {
        .top_k = 5,
        .min_score = 0.0f,
        .rrf_k = 60,
        .hybrid = true,
        .hybrid_weight = 0.7f
    };

    // 分块
    config.chunking = {
        .strategy = "recursive",
        .chunk_size = 500,
        .chunk_overlap = 50,
        .min_chunk_size = 50,
        .preserve_structure = true
    };

    // 日志
    config.logging = {
        .level = "info",
        .file = "./rag_data/logs/rag.log",
        .max_size = 100 * 1024 * 1024,
        .max_backups = 5,
        .console = true
    };

    // 服务器
    config.server = {
        .host = "0.0.0.0",
        .port = 8080,
        .num_threads = 4,
        .cors_enabled = true
    };

    return config;
}

std::string Config::to_string() const {
    std::ostringstream oss;
    oss << "RAG Config v" << version << "\n";
    oss << "  Data sources: " << data_sources.size() << "\n";
    oss << "  Embedding: " << embedding.model_type << "\n";
    oss << "  LLM: " << llm.model_type << "\n";
    oss << "  HNSW dim: " << hnsw.dim << "\n";
    oss << "  Chunk strategy: " << chunking.strategy << "\n";
    return oss.str();
}

// ========== ConfigLoader 实现 ==========

Config ConfigLoader::load(const std::filesystem::path& path) {
    RAG_INFO("Loading config from: " + path.string());

    std::ifstream file(path);
    if (!file.is_open()) {
        throw ConfigException(ErrorCodes::CONFIG_NOT_FOUND,
                             "Cannot open config file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_from_string(buffer.str());
}

Config ConfigLoader::load_from_string(const std::string& yaml_content) {
    Config config = Config::default_config();

    // 简单的 YAML 解析（实际项目中建议使用专用库如 yaml-cpp）
    std::istringstream iss(yaml_content);
    std::string line;

    while (std::getline(iss, line)) {
        // 跳过空行和注释
        if (line.empty() || line[0] == '#') continue;

        // 去除前后空白
        size_t start = line.find_first_not_of(" \t");
        size_t end = line.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);

        // 解析键值对 (简化版，不支持嵌套)
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // 去除引号
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        // 解析顶层配置
        if (key == "version") {
            config.version = value;
        } else if (key == "data_dir") {
            config.data_dir = value;
        } else if (key == "index_dir") {
            config.index_dir = value;
        } else if (key == "embedding_model" || key == "embedding.model_path") {
            config.embedding.model_path = value;
        } else if (key == "llm_model" || key == "llm.model_path") {
            config.llm.model_path = value;
        } else if (key == "chunk_size") {
            config.chunking.chunk_size = std::stoi(value);
        } else if (key == "chunk_overlap") {
            config.chunking.chunk_overlap = std::stoi(value);
        } else if (key == "top_k") {
            config.retrieval.top_k = std::stoi(value);
        } else if (key == "hnsw_dim" || key == "hnsw.dim") {
            config.hnsw.dim = std::stoi(value);
        }
    }

    // 应用环境变量覆盖
    apply_env_overrides(config);

    RAG_INFO("Config loaded successfully");
    return config;
}

Config ConfigLoader::load_from_env() {
    Config config = Config::default_config();
    apply_env_overrides(config);
    return config;
}

void ConfigLoader::save(const Config& config, const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw ConfigException(ErrorCodes::CONFIG_INVALID,
                             "Cannot write config file: " + path.string());
    }

    file << "# RAG System Configuration\n\n";
    file << "version: \"" << config.version << "\"\n\n";

    file << "# Data directories\n";
    file << "data_dir: \"" << config.data_dir << "\"\n";
    file << "index_dir: \"" << config.index_dir << "\"\n\n";

    file << "# Embedding model\n";
    file << "embedding.model_path: \"" << config.embedding.model_path << "\"\n";
    file << "embedding.batch_size: " << config.embedding.batch_size << "\n\n";

    file << "# LLM model\n";
    file << "llm.model_path: \"" << config.llm.model_path << "\"\n";
    file << "llm.max_tokens: " << config.llm.max_tokens << "\n\n";

    file << "# HNSW index\n";
    file << "hnsw.dim: " << config.hnsw.dim << "\n";
    file << "hnsw.m: " << config.hnsw.m << "\n\n";

    file << "# Chunking\n";
    file << "chunk_size: " << config.chunking.chunk_size << "\n";
    file << "chunk_overlap: " << config.chunking.chunk_overlap << "\n\n";

    file << "# Retrieval\n";
    file << "top_k: " << config.retrieval.top_k << "\n";
}

void ConfigLoader::apply_env_overrides(Config& config) {
    // 从环境变量读取配置
    if (const char* env = std::getenv("RAG_EMBEDDING_MODEL")) {
        config.embedding.model_path = env;
    }
    if (const char* env = std::getenv("RAG_LLM_MODEL")) {
        config.llm.model_path = env;
    }
    if (const char* env = std::getenv("RAG_DATA_DIR")) {
        config.data_dir = env;
        config.index_dir = std::string(env) + "/index";
        config.cache_dir = std::string(env) + "/cache";
    }
    if (const char* env = std::getenv("RAG_LOG_LEVEL")) {
        config.logging.level = env;
    }
    if (const char* env = std::getenv("RAG_SERVER_PORT")) {
        config.server.port = std::stoi(env);
    }
}

}  // namespace rag
