#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>

namespace rag {

// ========== 文件状态 ==========

struct FileState {
    std::string path;
    size_t size = 0;
    int64_t modified_time = 0;
    std::string content_hash;
};

struct IndexDelta {
    std::vector<std::string> added_files;
    std::vector<std::string> modified_files;
    std::vector<std::string> deleted_files;
    int64_t timestamp = 0;
};

// ========== 增量索引器 ==========

class IncrementalIndexer {
public:
    IncrementalIndexer();

    // 检测文件变化
    IndexDelta detect_changes(const std::string& base_path);

    // 应用增量
    void apply_delta(const IndexDelta& delta);

    // 索引单个文件
    void index_file(const std::string& path);

    // 移除文件索引
    void remove_file(const std::string& path);

    // 保存/加载索引状态
    void save_state(const std::string& state_file);
    bool load_state(const std::string& state_file);

private:
    std::unordered_map<std::string, FileState> previous_state_;
    std::string base_path_;
};

// ========== 实时索引器 ==========

class RealTimeIndexer {
public:
    RealTimeIndexer(std::shared_ptr<IncrementalIndexer> indexer);

    // 开始监听
    void start_watch(const std::string& path);

    // 停止监听
    void stop_watch();

    // 设置 debounce 延迟(ms)
    void set_debounce_ms(int ms) { debounce_ms_ = ms; }

    // 批量提交
    void flush();

private:
    std::shared_ptr<IncrementalIndexer> indexer_;
    int debounce_ms_ = 1000;
    bool watching_ = false;
};

// ========== 工厂函数 ==========

std::unique_ptr<IncrementalIndexer> create_incremental_indexer();
std::unique_ptr<RealTimeIndexer> create_realtime_indexer(
    std::shared_ptr<IncrementalIndexer> indexer);

}  // namespace rag