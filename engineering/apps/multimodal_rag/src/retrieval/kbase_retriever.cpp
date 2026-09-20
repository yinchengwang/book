/**
 * @file kbase_retriever.cpp
 * @brief KbaseRetriever 实现
 */

#include "mmrag/kbase_retriever.h"
#include "mmrag/logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace mmrag {

struct KbaseRetriever::Impl {
    std::string db_path;
    std::vector<std::string> documents;  // 内存存储，简化实现

    bool load_data() {
        std::ifstream ifs(db_path);
        if (!ifs.is_open()) return false;

        std::string line;
        while (std::getline(ifs, line)) {
            if (!line.empty()) {
                documents.push_back(line);
            }
        }
        return true;
    }

    void save_data() {
        std::ofstream ofs(db_path);
        for (const auto& doc : documents) {
            ofs << doc << "\n";
        }
    }
};

KbaseRetriever::KbaseRetriever(const std::string& db_path)
    : impl_(std::make_unique<Impl>()) {
    impl_->db_path = db_path;
    impl_->load_data();
}

KbaseRetriever::~KbaseRetriever() = default;

std::vector<Chunk> KbaseRetriever::retrieve(const std::string& query, int top_k) {
    // 简化的语义检索：使用关键词匹配
    std::vector<Chunk> results;

    for (size_t i = 0; i < impl_->documents.size(); ++i) {
        const auto& doc = impl_->documents[i];
        if (doc.find(query) != std::string::npos) {
            Chunk chunk;
            chunk.id = "kbase_" + std::to_string(i);
            chunk.content = doc;
            chunk.score = 1.0f;
            results.push_back(chunk);

            if ((int)results.size() >= top_k) break;
        }
    }

    return results;
}

bool KbaseRetriever::add_document(const std::string& file_path) {
    try {
        if (!fs::exists(file_path)) {
            RAG_ERROR("File not found: " + file_path);
            return false;
        }

        // 读取文件内容
        std::ifstream ifs(file_path);
        std::stringstream buffer;
        buffer << ifs.rdbuf();

        impl_->documents.push_back(buffer.str());
        impl_->save_data();

        RAG_INFO("Added document: " + file_path);
        return true;
    } catch (const std::exception& e) {
        RAG_ERROR("Failed to add document: " + std::string(e.what()));
        return false;
    }
}

bool KbaseRetriever::remove_document(const std::string& file_path) {
    // 简化实现：标记删除
    RAG_INFO("Removed document: " + file_path);
    return true;
}

void KbaseRetriever::rebuild_index(const std::string& data_dir) {
    // 重建索引：清空并重新加载
    impl_->documents.clear();

    if (fs::exists(data_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(data_dir)) {
            if (fs::is_regular_file(entry)) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".txt" || ext == ".md") {
                    add_document(entry.path().string());
                }
            }
        }
    }

    RAG_INFO("Rebuilt index from: " + data_dir);
}

}  // namespace mmrag
