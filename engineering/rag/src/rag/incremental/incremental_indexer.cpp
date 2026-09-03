/**
 * @file incremental_indexer.cpp
 * @brief 增量索引实现
 */

#include "rag/incremental_indexer.h"
#include "rag/logger.h"
#include <algorithm>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>

namespace rag {

// ========== 工具函数 ==========

static std::string get_file_hash(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";

    // 简化版：读取前 8KB 内容做 hash
    char buffer[8192];
    file.read(buffer, sizeof(buffer));
    std::streamsize bytes_read = file.gcount();

    // 简单 hash：使用 size + 修改时间 + 内容前缀
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return "";

    std::string result = std::to_string(st.st_size) + "_" + std::to_string(st.st_mtime);
    for (std::streamsize i = 0; i < bytes_read; ++i) {
        result += buffer[i];
    }
    return result;
}

static int64_t get_modified_time(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<int64_t>(st.st_mtime);
}

static size_t get_file_size(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<size_t>(st.st_size);
}

static void scan_directory(const std::string& base_path,
                           std::vector<std::string>& files) {
    DIR* dir = opendir(base_path.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string full_path = base_path + "/" + name;

        struct stat st;
        if (stat(full_path.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                scan_directory(full_path, files);
            } else if (S_ISREG(st.st_mode)) {
                files.push_back(full_path);
            }
        }
    }
    closedir(dir);
}

// ========== IncrementalIndexer 实现 ==========

IncrementalIndexer::IncrementalIndexer() : base_path_("") {}

IndexDelta IncrementalIndexer::detect_changes(const std::string& base_path) {
    IndexDelta delta;
    delta.timestamp = static_cast<int64_t>(time(nullptr));
    base_path_ = base_path;

    // 扫描当前目录
    std::vector<std::string> current_files;
    scan_directory(base_path, current_files);

    // 构建当前状态
    std::unordered_map<std::string, FileState> current_state;
    for (const auto& file : current_files) {
        FileState state;
        state.path = file;
        state.size = get_file_size(file);
        state.modified_time = get_modified_time(file);
        state.content_hash = get_file_hash(file);
        current_state[file] = state;
    }

    // 对比上次状态
    for (const auto& kv : current_state) {
        const std::string& path = kv.first;
        const FileState& current = kv.second;

        auto prev_it = previous_state_.find(path);
        if (prev_it == previous_state_.end()) {
            // 新增文件
            delta.added_files.push_back(path);
        } else {
            const FileState& prev = prev_it->second;
            // 检查是否修改：大小或修改时间变化
            if (current.size != prev.size ||
                current.modified_time != prev.modified_time) {
                delta.modified_files.push_back(path);
            }
        }
    }

    // 检测删除的文件
    for (const auto& kv : previous_state_) {
        const std::string& path = kv.first;
        if (current_state.find(path) == current_state.end()) {
            delta.deleted_files.push_back(path);
        }
    }

    // 更新状态
    previous_state_ = std::move(current_state);

    return delta;
}

void IncrementalIndexer::apply_delta(const IndexDelta& delta) {
    // 新增文件
    for (const auto& path : delta.added_files) {
        index_file(path);
    }

    // 修改文件：先删除再索引
    for (const auto& path : delta.modified_files) {
        remove_file(path);
        index_file(path);
    }

    // 删除文件
    for (const auto& path : delta.deleted_files) {
        remove_file(path);
    }
}

void IncrementalIndexer::index_file(const std::string& path) {
    RAG_INFO("Indexing file: " + path);
    // TODO: 调用实际的索引逻辑
}

void IncrementalIndexer::remove_file(const std::string& path) {
    RAG_INFO("Removing file from index: " + path);
    // TODO: 调用实际的删除逻辑
}

void IncrementalIndexer::save_state(const std::string& state_file) {
    std::ofstream file(state_file);
    if (!file) {
        RAG_ERROR("Failed to open state file for writing: " + state_file);
        return;
    }

    file << "{\n";
    file << "  \"files\": [\n";

    bool first = true;
    for (const auto& kv : previous_state_) {
        if (!first) file << ",\n";
        first = false;

        const FileState& state = kv.second;
        file << "    {\n";
        file << "      \"path\": \"" << state.path << "\",\n";
        file << "      \"size\": " << state.size << ",\n";
        file << "      \"modified_time\": " << state.modified_time << ",\n";
        file << "      \"content_hash\": \"" << state.content_hash << "\"\n";
        file << "    }";
    }

    file << "\n  ]\n";
    file << "}\n";
    file.close();
    RAG_INFO("State saved to: " + state_file);
}

bool IncrementalIndexer::load_state(const std::string& state_file) {
    std::ifstream file(state_file);
    if (!file) {
        RAG_WARN("Failed to open state file for reading: " + state_file);
        return false;
    }

    // 解析简单的 JSON
    previous_state_.clear();
    std::string line;
    std::string current_path;
    FileState state;
    bool in_files = false;
    bool in_object = false;

    while (std::getline(file, line)) {
        // 简单解析：查找 "path", "size", "modified_time", "content_hash"
        if (line.find("\"files\"") != std::string::npos) {
            in_files = true;
        }
        if (in_files && line.find("\"path\"") != std::string::npos) {
            size_t start = line.find("\"") + 1;
            size_t end = line.rfind("\"");
            if (start < end) {
                current_path = line.substr(start, end - start);
                state.path = current_path;
                in_object = true;
            }
        }
        if (in_object && line.find("\"size\"") != std::string::npos) {
            size_t start = line.find(":") + 1;
            state.size = std::stoll(line.substr(start));
        }
        if (in_object && line.find("\"modified_time\"") != std::string::npos) {
            size_t start = line.find(":") + 1;
            state.modified_time = std::stoll(line.substr(start));
        }
        if (in_object && line.find("\"content_hash\"") != std::string::npos) {
            size_t start = line.find("\"") + 1;
            size_t end = line.rfind("\"");
            if (start < end) {
                state.content_hash = line.substr(start, end - start);
            }
        }
        if (in_object && line.find("}") != std::string::npos) {
            if (!state.path.empty()) {
                previous_state_[state.path] = state;
            }
            state = FileState();
            in_object = false;
        }
    }

    file.close();
    RAG_INFO("State loaded from: " + state_file + " (" + std::to_string(previous_state_.size()) + " files)");
    return true;
}

// ========== RealTimeIndexer 实现 ==========

RealTimeIndexer::RealTimeIndexer(std::shared_ptr<IncrementalIndexer> indexer)
    : indexer_(indexer), debounce_ms_(1000), watching_(false) {}

void RealTimeIndexer::start_watch(const std::string& path) {
    if (watching_) {
        RAG_WARN("Already watching");
        return;
    }

    watching_ = true;
    RAG_INFO("Started watching: " + path);
    // TODO: 实际的文件系统监听实现（inotify/FSEvents）
}

void RealTimeIndexer::stop_watch() {
    if (!watching_) {
        return;
    }

    watching_ = false;
    RAG_INFO("Stopped watching");
    flush();
}

void RealTimeIndexer::flush() {
    RAG_INFO("Flushing pending changes");
    // TODO: 提交所有待处理的变更
}

// ========== 工厂函数 ==========

std::unique_ptr<IncrementalIndexer> create_incremental_indexer() {
    return std::make_unique<IncrementalIndexer>();
}

std::unique_ptr<RealTimeIndexer> create_realtime_indexer(
    std::shared_ptr<IncrementalIndexer> indexer) {
    return std::make_unique<RealTimeIndexer>(indexer);
}

}  // namespace rag