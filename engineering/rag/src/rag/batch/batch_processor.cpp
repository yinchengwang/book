// engineering/rag/src/rag/batch/batch_processor.cpp

#include "rag/batch_processor.h"
#include "rag/gpu_config.h"
#include "rag/logger.h"
#include "rag/chunker.h"
#include "rag/embedding.h"
#include "rag/vector_index.h"
#include "rag/bm25_index.h"
#include "rag/database.h"
#include "rag/parser.h"
#include <algorithm>
#include <numeric>

namespace rag {

struct BatchProcessor::Impl {
    Impl(const BatchConfig& cfg) : config(cfg) {}

    BatchConfig config;

    // 组件
    std::shared_ptr<Chunker> chunker;
    std::shared_ptr<EmbeddingService> embed_service;
    std::shared_ptr<VectorIndex> vector_index;
    std::shared_ptr<BM25Index> bm25_index;
    std::shared_ptr<Database> database;

    // 统计
    std::atomic<uint64_t> total_batches{0};
    std::atomic<uint64_t> total_documents{0};
    std::atomic<uint64_t> total_chunks{0};
    std::vector<double> batch_times_ms;
};

// ========== StreamingIndexBuilder::Impl ==========

class StreamingIndexBuilder::Impl {
public:
    Impl(std::shared_ptr<BatchProcessor> p, std::shared_ptr<ParserRegistry> par)
        : processor(p), parser(par) {}

    std::shared_ptr<BatchProcessor> processor;
    std::shared_ptr<ParserRegistry> parser;
    std::function<void(const BatchProgress&)> progress_callback;
    std::function<bool(const std::string&)> file_filter;
    std::atomic<bool> running{false};
};

BatchProcessor::BatchProcessor(const BatchConfig& config)
    : impl_(std::make_unique<Impl>(config)), config_(config) {}

BatchProcessor::~BatchProcessor() = default;

void BatchProcessor::set_chunker(std::shared_ptr<Chunker> chunker) {
    impl_->chunker = chunker;
}

void BatchProcessor::set_embedding_service(std::shared_ptr<EmbeddingService> embed) {
    impl_->embed_service = embed;
}

void BatchProcessor::set_vector_index(std::shared_ptr<VectorIndex> index) {
    impl_->vector_index = index;
}

void BatchProcessor::set_bm25_index(std::shared_ptr<BM25Index> bm25) {
    impl_->bm25_index = bm25;
}

void BatchProcessor::set_database(std::shared_ptr<Database> db) {
    impl_->database = db;
}

BatchResult BatchProcessor::process_batch(const std::vector<Document>& docs) {
    auto start = std::chrono::high_resolution_clock::now();
    BatchResult result;

    try {
        // 1. 解析和分块
        std::vector<Chunk> chunks;
        for (const auto& doc : docs) {
            if (!impl_->chunker) {
                throw std::runtime_error("Chunker not set");
            }
            auto doc_chunks = impl_->chunker->chunk(doc.content, doc.id, doc.metadata);
            chunks.insert(chunks.end(), doc_chunks.chunks.begin(), doc_chunks.chunks.end());
        }

        result.chunks_created = static_cast<int>(chunks.size());
        result.documents_processed = static_cast<int>(docs.size());

        // 2. GPU 批量编码
        if (impl_->embed_service) {
            std::vector<std::string> texts;
            for (const auto& chunk : chunks) {
                texts.push_back(chunk.content);
            }

            // 分批编码（避免显存溢出）
            std::vector<std::vector<float>> all_embeddings;
            for (size_t i = 0; i < texts.size(); i += config_.gpu_batch_size) {
                size_t end = std::min(i + config_.gpu_batch_size, texts.size());
                std::vector<std::string> batch_texts(texts.begin() + i, texts.begin() + end);
                auto batch_embeddings = impl_->embed_service->encode_batch(batch_texts);
                all_embeddings.insert(all_embeddings.end(),
                    batch_embeddings.begin(), batch_embeddings.end());
            }

            // 3. 批量添加到向量索引
            if (impl_->vector_index) {
                std::vector<std::string> ids;
                for (const auto& chunk : chunks) {
                    ids.push_back(chunk.id);
                }
                impl_->vector_index->add_batch(ids, all_embeddings);
            }
        }

        // 4. 批量添加到 BM25
        if (impl_->bm25_index) {
            for (const auto& chunk : chunks) {
                impl_->bm25_index->add(chunk.id, chunk.content);
            }
        }

        // 5. 批量存储到数据库
        if (impl_->database) {
            ChunkRepository repo(*impl_->database);
            repo.insert_batch(chunks);
        }

        // 6. 提取 chunk IDs
        for (const auto& chunk : chunks) {
            result.chunk_ids.push_back(chunk.id);
        }

        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        RAG_ERROR("Batch processing failed: " + result.error_message);
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // 更新统计
    impl_->total_batches++;
    impl_->total_documents += result.documents_processed;
    impl_->total_chunks += result.chunks_created;

    return result;
}

std::future<BatchResult> BatchProcessor::process_batch_async(
    const std::vector<Document>& docs) {

    return std::async(std::launch::async, [this, docs]() {
        return process_batch(docs);
    });
}

BatchProcessor::Stats BatchProcessor::get_stats() const {
    Stats stats;
    stats.total_batches = impl_->total_batches.load();
    stats.total_documents = impl_->total_documents.load();
    stats.total_chunks = impl_->total_chunks.load();
    return stats;
}

void BatchProcessor::update_config(const BatchConfig& config) {
    config_ = config;
    impl_->config = config;
}

void BatchProcessor::index_directory(const std::string& path, int batch_size,
                                     std::function<void(const BatchProgress&)> progress_callback) {
    // 扫描文件
    std::vector<std::string> files;
    std::filesystem::recursive_directory_iterator it(path);
    for (const auto& entry : it) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path().string());
        }
    }

    BatchProgress progress;
    progress.total_files = static_cast<int>(files.size());
    progress.start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    std::vector<Document> batch;

    for (const auto& file : files) {
        try {
            Document doc;
            doc.id = generate_uuid();
            doc.content = "placeholder content";  // 实际应该用 parser 解析
            doc.metadata.file_path = file;
            batch.push_back(doc);

            if (static_cast<int>(batch.size()) >= batch_size) {
                auto result = process_batch(batch);

                progress.processed_files += result.documents_processed;
                progress.total_chunks += result.chunks_created;
                progress.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count()
                    - progress.start_time_ms;
                progress.docs_per_second = progress.processed_files * 1000.0 /
                    std::max(progress.elapsed_ms, (int64_t)1);

                if (progress_callback) {
                    progress_callback(progress);
                }

                batch.clear();
            }
        } catch (const std::exception& e) {
            RAG_WARN("Failed to process file: " + file + " - " + e.what());
        }
    }

    // 处理剩余
    if (!batch.empty()) {
        process_batch(batch);
    }
}

// ========== StreamingIndexBuilder ==========

StreamingIndexBuilder::StreamingIndexBuilder(
    std::shared_ptr<BatchProcessor> processor,
    std::shared_ptr<ParserRegistry> parser)
    : impl_(std::make_unique<Impl>(processor, parser)) {}

StreamingIndexBuilder::~StreamingIndexBuilder() {
    stop();
}

void StreamingIndexBuilder::build(const std::string& path, int batch_size) {
    impl_->running = true;

    auto files = scan_files(path);
    BatchProgress progress;
    progress.total_files = static_cast<int>(files.size());
    progress.start_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    std::vector<Document> batch;

    for (const auto& file : files) {
        if (!impl_->running) break;

        try {
            auto* parser = impl_->parser->get_parser(file);
            if (!parser) {
                RAG_WARN("No parser for file: " + file);
                continue;
            }
            auto parse_result = parser->parse(file);

            Document doc;
            doc.id = generate_uuid();
            doc.content = parse_result.content;
            doc.metadata.file_path = file;
            doc.metadata.title = parse_result.title;
            batch.push_back(doc);

            if (static_cast<int>(batch.size()) >= batch_size) {
                auto result = impl_->processor->process_batch(batch);

                progress.processed_files += result.documents_processed;
                progress.total_chunks += result.chunks_created;
                progress.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count()
                    - progress.start_time_ms;
                progress.docs_per_second = progress.processed_files * 1000.0 /
                    std::max(progress.elapsed_ms, (int64_t)1);

                if (impl_->progress_callback) {
                    impl_->progress_callback(progress);
                }

                batch.clear();
            }
        } catch (const std::exception& e) {
            RAG_WARN("Failed to process file: " + file + " - " + e.what());
        }
    }

    // 处理剩余
    if (!batch.empty() && impl_->running) {
        impl_->processor->process_batch(batch);
    }

    impl_->running = false;
}

void StreamingIndexBuilder::set_progress_callback(
    std::function<void(const BatchProgress&)> cb) {
    impl_->progress_callback = cb;
}

void StreamingIndexBuilder::set_filter(std::function<bool(const std::string&)> filter) {
    impl_->file_filter = filter;
}

void StreamingIndexBuilder::stop() {
    impl_->running = false;
}

std::vector<std::string> StreamingIndexBuilder::scan_files(const std::string& path) {
    std::vector<std::string> files;
    std::filesystem::recursive_directory_iterator it(path);

    for (const auto& entry : it) {
        if (entry.is_regular_file()) {
            if (!impl_->file_filter || impl_->file_filter(entry.path().string())) {
                files.push_back(entry.path().string());
            }
        }
    }

    return files;
}

}  // namespace rag