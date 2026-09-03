// engineering/rag/include/rag/batch_processor.h
#pragma once

#include "rag/types.h"
#include "rag/config.h"
#include <memory>
#include <vector>
#include <string>
#include <future>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>

namespace rag {

// Forward declarations
class Chunker;
class EmbeddingService;
class VectorIndex;
class BM25Index;
class Database;
class ParserRegistry;

struct BatchConfig {
    int gpu_batch_size = 32;        // GPU 批量大小
    int cpu_prefetch_threads = 4;   // CPU 预取线程
    bool use_fp16 = true;           // 半精度
    int max_queue_size = 100;       // 最大队列长度
    bool stream_output = true;      // 流式输出进度
};

struct BatchProgress {
    int total_files = 0;
    int processed_files = 0;
    int total_chunks = 0;
    int64_t start_time_ms = 0;
    int64_t elapsed_ms = 0;
    float docs_per_second = 0.0f;
};

struct BatchResult {
    bool success = false;
    std::vector<std::string> chunk_ids;
    int documents_processed = 0;
    int chunks_created = 0;
    int64_t duration_ms = 0;
    std::string error_message;
};

class BatchProcessor {
public:
    explicit BatchProcessor(const BatchConfig& config);
    ~BatchProcessor();

    // 设置组件
    void set_chunker(std::shared_ptr<Chunker> chunker);
    void set_embedding_service(std::shared_ptr<EmbeddingService> embed);
    void set_vector_index(std::shared_ptr<VectorIndex> index);
    void set_bm25_index(std::shared_ptr<BM25Index> bm25);
    void set_database(std::shared_ptr<Database> db);

    // 批量处理
    BatchResult process_batch(const std::vector<Document>& docs);

    // 异步批量处理
    std::future<BatchResult> process_batch_async(const std::vector<Document>& docs);

    // 流式索引目录
    void index_directory(const std::string& path, int batch_size = 32,
                        std::function<void(const BatchProgress&)> progress_callback = nullptr);

    // 配置
    void update_config(const BatchConfig& config);
    const BatchConfig& config() const { return config_; }

    // 统计
    struct Stats {
        uint64_t total_batches = 0;
        uint64_t total_documents = 0;
        uint64_t total_chunks = 0;
        double avg_batch_time_ms = 0.0;
        double avg_throughput = 0.0;
    };
    Stats get_stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    BatchConfig config_;
};

// ========== 流式索引构建器 ==========

class StreamingIndexBuilder {
public:
    StreamingIndexBuilder(std::shared_ptr<BatchProcessor> processor,
                         std::shared_ptr<ParserRegistry> parser);
    ~StreamingIndexBuilder();

    void build(const std::string& path, int batch_size = 32);

    void set_progress_callback(std::function<void(const BatchProgress&)> cb);
    void set_filter(std::function<bool(const std::string& path)> filter);

    bool is_running() const;
    void stop();

private:
    std::vector<std::string> scan_files(const std::string& path);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rag