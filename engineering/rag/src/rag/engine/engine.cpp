/**
 * @file engine.cpp
 * @brief RAG 核心引擎实现
 */

#include "rag/engine.h"
#include "rag/graph_support.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <fstream>

namespace rag {

// ========== RAGEngine 实现 ==========

RAGEngine::RAGEngine() = default;

RAGEngine::~RAGEngine() {
    shutdown();
}

void RAGEngine::init(const EngineConfig& engine_config) {
    require_state(EngineState::UNINITIALIZED, "init");

    state_ = EngineState::INITIALIZING;
    config_ = engine_config.config;

    RAG_INFO("Initializing RAG Engine...");

    // 创建目录
    if (engine_config.auto_create_dirs) {
        create_directories();
    }

    // 初始化数据库
    auto db_path = data_dir_ / "rag.db";
    db_ = std::make_shared<Database>();
    db_->open(db_path.string());

    // 初始化 Schema
    SchemaManager schema(*db_);
    schema.init();

    // 初始化 Repository
    doc_repo_ = std::make_shared<DocumentRepository>(*db_);
    chunk_repo_ = std::make_shared<ChunkRepository>(*db_);
    status_repo_ = std::make_shared<IndexStatusRepository>(*db_);

    // 初始化 Graph RAG 扩展
    if (config_.graph.enabled) {
        graph_config_ = config_.graph;
        graph_config_.kg_storage_path = (index_dir_ / config_.graph.kg_storage_path).string();
        graph_ext_ = create_graph_extension(db_, graph_config_, nullptr);
        graph_ext_->load_knowledge_graph();
        graph_ext_->set_text_retriever(retriever_);
        RAG_INFO("Graph RAG enabled");
    }

    // 初始化组件
    init_components();

    // 加载已有索引
    if (engine_config.load_existing_index && std::filesystem::exists(vector_index_path_)) {
        try {
            vector_index_->load(vector_index_path_);
            bm25_index_->load(bm25_index_path_);
            RAG_INFO("Loaded existing index");
        } catch (const std::exception& e) {
            RAG_WARN("Failed to load existing index: " + std::string(e.what()));
        }
    }

    state_ = EngineState::READY;
    RAG_INFO("RAG Engine initialized successfully");
}

void RAGEngine::shutdown() {
    if (state_ == EngineState::UNINITIALIZED) {
        return;
    }

    RAG_INFO("Shutting down RAG Engine...");

    // 保存索引
    if (vector_index_ && vector_index_->size() > 0) {
        save_index();
    }

    // 保存知识图谱
    if (graph_ext_ && graph_ext_->is_graph_enabled()) {
        graph_ext_->save_knowledge_graph();
    }

    // 清理
    graph_ext_.reset();
    retriever_.reset();
    embed_service_.reset();
    bm25_index_.reset();
    vector_index_.reset();
    parser_registry_.reset();
    chunker_.reset();
    status_repo_.reset();
    chunk_repo_.reset();
    doc_repo_.reset();
    db_->close();
    db_.reset();

    state_ = EngineState::UNINITIALIZED;
    RAG_INFO("RAG Engine shutdown complete");
}

void RAGEngine::create_directories() {
    data_dir_ = config_.data_dir;
    index_dir_ = config_.index_dir;

    std::filesystem::create_directories(data_dir_);
    std::filesystem::create_directories(index_dir_);

    vector_index_path_ = index_dir_ / "vectors.bin";
    bm25_index_path_ = index_dir_ / "bm25.bin";
}

void RAGEngine::init_components() {
    // 初始化分块器
    chunker_ = ChunkerFactory::create(config_.chunking);

    // 初始化解析器
    parser_registry_ = std::make_shared<ParserRegistry>();

    // 初始化向量索引
    vector_index_ = create_vector_index(config_.hnsw);
    vector_index_->init(config_.hnsw);

    // 初始化 BM25 索引
    bm25_index_ = create_bm25_index(config_.bm25);
    bm25_index_->init(config_.bm25);

    // 初始化 Embedding 服务
    embed_service_ = std::make_shared<SimpleEmbeddingService>(config_.hnsw.dim);
    static_cast<SimpleEmbeddingService*>(embed_service_.get())->load();

    // 初始化检索器
    retriever_ = create_retriever(
        config_.retrieval,
        vector_index_,
        bm25_index_,
        embed_service_
    );

    RAG_DEBUG("Components initialized");
}

IndexBuildResult RAGEngine::build_index(const std::vector<std::string>& paths) {
    require_state(EngineState::READY, "build_index");

    auto start = std::chrono::steady_clock::now();
    state_ = EngineState::INDEXING;

    IndexBuildResult result;

    // 获取要处理的文件
    std::vector<std::filesystem::path> files;
    if (paths.empty()) {
        for (const auto& source : config_.data_sources) {
            auto source_files = scan_files(source.path);
            files.insert(files.end(), source_files.begin(), source_files.end());
        }
    } else {
        for (const auto& path : paths) {
            auto source_files = scan_files(path);
            files.insert(files.end(), source_files.begin(), source_files.end());
        }
    }

    RAG_INFO("Found " + std::to_string(files.size()) + " files to index");

    // 处理文件
    for (const auto& file : files) {
        try {
            if (process_file(file)) {
                result.documents_processed++;
            } else {
                result.documents_failed++;
            }
        } catch (const std::exception& e) {
            RAG_ERROR("Failed to process file: " + file.string() + " - " + e.what());
            result.documents_failed++;
        }

        // 更新进度
        if ((result.documents_processed + result.documents_failed) % 10 == 0) {
            RAG_INFO("Progress: " + std::to_string(result.documents_processed) +
                     " processed, " + std::to_string(result.documents_failed) + " failed");
        }
    }

    // 统计
    result.chunks_created = static_cast<int>(chunk_repo_->count());
    result.success = true;

    // 更新索引状态
    auto status = get_index_status();
    status.status = IndexStatus::Status::READY;
    status.last_update = current_timestamp();
    status_repo_->upsert(status);

    auto end = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    state_ = EngineState::READY;

    RAG_INFO("Index build complete: " + std::to_string(result.documents_processed) +
             " docs, " + std::to_string(result.chunks_created) + " chunks, " +
             std::to_string(result.duration_ms) + "ms");

    return result;
}

IndexBuildResult RAGEngine::update_index(const std::string& path) {
    require_state(EngineState::READY, "update_index");

    IndexBuildResult result;

    try {
        auto file_path = std::filesystem::path(path);
        if (process_file(file_path)) {
            result.documents_processed = 1;
            result.success = true;
        } else {
            result.documents_failed = 1;
        }
    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    return result;
}

void RAGEngine::clear_index() {
    require_state(EngineState::READY, "clear_index");

    vector_index_->clear();
    bm25_index_->clear();

    // 删除所有文档和块
    db_->execute("DELETE FROM chunks");
    db_->execute("DELETE FROM documents");
    db_->execute("DELETE FROM embeddings");

    // 删除索引文件
    if (std::filesystem::exists(vector_index_path_)) {
        std::filesystem::remove(vector_index_path_);
    }
    if (std::filesystem::exists(bm25_index_path_)) {
        std::filesystem::remove(bm25_index_path_);
    }

    RAG_INFO("Index cleared");
}

QueryResult RAGEngine::query(const std::string& query, int top_k) {
    require_state(EngineState::READY, "query");

    auto start = std::chrono::steady_clock::now();
    state_ = EngineState::QUERYING;

    QueryResult result;
    result.request_id = generate_uuid();

    // 1. 检索相关块
    auto retrieval_results = retrieve(query, top_k);

    if (retrieval_results.empty()) {
        result.answer = "抱歉，没有找到与您问题相关的文档。";
        state_ = EngineState::READY;
        return result;
    }

    result.chunks = retrieval_results;

    // 2. 构建上下文
    std::string context;
    for (size_t i = 0; i < retrieval_results.size(); ++i) {
        const auto& chunk = retrieval_results[i].chunk;
        context += "【来源 " + std::to_string(i + 1) + "】\n";
        context += chunk.content + "\n\n";
    }

    // 3. 生成答案（简化版本，返回检索结果）
    // 生产环境应调用 LLM
    result.answer = "基于检索到的文档，以下是相关信息：\n\n";
    result.answer += context;

    // 计算置信度
    if (!retrieval_results.empty()) {
        float total_score = 0;
        for (const auto& r : retrieval_results) {
            total_score += r.score;
        }
        result.confidence = total_score / retrieval_results.size();
    }

    auto end = std::chrono::steady_clock::now();
    result.query_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    result.timestamp = current_timestamp();

    state_ = EngineState::READY;
    return result;
}

std::vector<RetrievalResult> RAGEngine::retrieve(const std::string& query, int top_k) {
    require_state(EngineState::READY, "retrieve");

    // 调用检索器
    return retriever_->retrieve(query, top_k);
}

void RAGEngine::add_document(const Document& doc) {
    require_state(EngineState::READY, "add_document");

    doc_repo_->insert(doc);

    // 解析和分块
    auto* parser = parser_registry_->get_parser(doc.metadata.file_path);
    auto parse_result = parser->parse_content(doc.content);

    // 分块
    auto chunk_result = chunker_->chunk(parse_result.content, doc.id, doc.metadata);

    // 存储块
    for (auto& chunk : chunk_result.chunks) {
        chunk_repo_->insert(chunk);

        // 添加到检索索引
        auto vector = embed_service_->encode(chunk.content);
        vector_index_->add(chunk.id, vector);
        bm25_index_->add(chunk.id, chunk.content);
    }
}

void RAGEngine::remove_document(const std::string& doc_id) {
    require_state(EngineState::READY, "remove_document");

    // 获取文档的所有块
    auto chunks = chunk_repo_->find_by_document(doc_id);

    // 从索引中删除
    for (const auto& chunk : chunks) {
        vector_index_->remove(chunk.id);
    }

    // 从数据库删除
    chunk_repo_->remove_by_document(doc_id);
    doc_repo_->remove(doc_id);
}

std::optional<Document> RAGEngine::get_document(const std::string& doc_id) {
    return doc_repo_->find_by_id(doc_id);
}

std::vector<Document> RAGEngine::list_documents() {
    return doc_repo_->find_all();
}

IndexStatus RAGEngine::get_index_status() const {
    IndexStatus status;
    status.index_name = "main";

    try {
        auto existing = status_repo_->get("main");
        if (existing) {
            status = *existing;
        }
    } catch (...) {}

    status.document_count = doc_repo_->count();
    status.chunk_count = chunk_repo_->count();
    status.vector_count = vector_index_->size();
    status.index_size = vector_index_->is_loaded() ?
        std::filesystem::file_size(vector_index_path_) : 0;

    if (state_ == EngineState::READY) {
        status.status = IndexStatus::Status::READY;
    } else if (state_ == EngineState::INDEXING) {
        status.status = IndexStatus::Status::BUILDING;
    }

    return status;
}

void RAGEngine::save_index(const std::filesystem::path& path) {
    if (!path.empty()) {
        vector_index_->save(path / "vectors.bin");
        bm25_index_->save(path / "bm25.bin");
    } else {
        vector_index_->save(vector_index_path_);
        bm25_index_->save(bm25_index_path_);
    }
}

void RAGEngine::load_index(const std::filesystem::path& path) {
    auto vec_path = path.empty() ? vector_index_path_ : path / "vectors.bin";
    auto bm_path = path.empty() ? bm25_index_path_ : path / "bm25.bin";

    vector_index_->load(vec_path);
    bm25_index_->load(bm_path);
}

GraphBuildResult RAGEngine::build_knowledge_graph() {
    if (!graph_ext_) {
        return GraphBuildResult{.success = false, .error_message = "Graph not enabled"};
    }
    return graph_ext_->build_knowledge_graph();
}

GraphBuildResult RAGEngine::update_knowledge_graph(const std::vector<std::string>& chunk_ids) {
    if (!graph_ext_) {
        return GraphBuildResult{.success = false, .error_message = "Graph not enabled"};
    }
    return graph_ext_->update_knowledge_graph(chunk_ids);
}

GraphRetrievalResult RAGEngine::graph_retrieve(const std::string& query, int top_k) {
    if (!graph_ext_) {
        return GraphRetrievalResult();
    }
    return graph_ext_->graph_retrieve(query, top_k);
}

bool RAGEngine::is_graph_enabled() const {
    return graph_ext_ != nullptr && graph_ext_->is_graph_enabled();
}

std::vector<std::filesystem::path> RAGEngine::scan_files(const std::string& path) {
    std::vector<std::filesystem::path> files;
    auto base_path = std::filesystem::path(path);

    if (!std::filesystem::exists(base_path)) {
        RAG_WARN("Path does not exist: " + path);
        return files;
    }

    if (std::filesystem::is_regular_file(base_path)) {
        files.push_back(base_path);
        return files;
    }

    // 扫描目录
    for (const auto& entry : std::filesystem::recursive_directory_iterator(base_path)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            // 支持的文件类型
            if (ext == ".md" || ext == ".markdown" ||
                ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c" ||
                ext == ".py" || ext == ".java" || ext == ".txt") {
                files.push_back(entry.path());
            }
        }
    }

    return files;
}

bool RAGEngine::process_file(const std::filesystem::path& file_path) {
    // 检查是否已处理且未修改
    auto existing = doc_repo_->find_by_path(file_path.string());
    if (existing) {
        auto file_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::filesystem::last_write_time(file_path).time_since_epoch()).count();

        if (file_time <= existing->metadata.modified_time) {
            RAG_DEBUG("Skipping unchanged file: " + file_path.string());
            return true;
        }

        // 文件已修改，删除旧记录
        remove_document(existing->id);
    }

    // 解析文件
    auto* parser = parser_registry_->get_parser(file_path);
    auto parse_result = parser->parse(file_path);

    // 创建文档
    Document doc;
    doc.id = generate_uuid();
    doc.content = parse_result.content;
    doc.metadata = get_file_metadata(file_path);
    if (!parse_result.title.empty()) {
        doc.metadata.title = parse_result.title;
    }
    doc.created_at = current_timestamp();
    doc.updated_at = current_timestamp();
    doc.status = Document::Status::PROCESSING;

    // 保存文档
    doc_repo_->insert(doc);

    // 分块
    auto chunk_result = chunker_->chunk(doc.content, doc.id, doc.metadata);

    // 存储块和索引
    int vectors_added = 0;
    for (auto& chunk : chunk_result.chunks) {
        chunk_repo_->insert(chunk);

        // 添加到向量索引
        auto vector = embed_service_->encode(chunk.content);
        vector_index_->add(chunk.id, vector);

        // 添加到 BM25 索引
        bm25_index_->add(chunk.id, chunk.content);
        vectors_added++;
    }

    // 更新文档状态
    doc.status = Document::Status::INDEXED;
    doc.indexed_at = current_timestamp();
    doc_repo_->update(doc);

    RAG_DEBUG("Processed: " + file_path.filename().string() +
              " (" + std::to_string(chunk_result.chunks.size()) + " chunks)");

    return true;
}

void RAGEngine::require_state(EngineState expected, const std::string& operation) {
    if (state_ != expected) {
        throw RAGException(ErrorCodes::INTERNAL_ERROR,
                          "Invalid state for operation: " + operation +
                          ". Expected: " + std::to_string(static_cast<int>(expected)) +
                          ", Got: " + std::to_string(static_cast<int>(state_)));
    }
}

// ========== 工厂函数 ==========

std::unique_ptr<RAGEngine> create_engine(const EngineConfig& config) {
    auto engine = std::make_unique<RAGEngine>();
    engine->init(config);
    return engine;
}

}  // namespace rag
