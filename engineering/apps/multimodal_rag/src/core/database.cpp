/**
 * @file database.cpp
 * @brief 自研 KV 存储引擎实现
 *
 * 使用自研 KV 引擎替代 SQLite，通过 key 前缀命名空间实现表隔离。
 * 数据以 JSON 格式序列化存储。
 */

#include "mmrag/database.h"
#include "mmrag/logger.h"
#include "mmrag/json.h"
#include "db/storage/kv/kv.h"
#include <stdexcept>
#include <sstream>
#include <mutex>
#include <algorithm>

namespace mmrag {

using json = nlohmann::json;

// ========== Database 实现 ==========

Database::Database() = default;

Database::~Database() {
    close();
}

Database::Database(Database&& other) noexcept
    : db_(other.db_), db_path_(std::move(other.db_path_)) {
    other.db_ = nullptr;
    other.db_path_.clear();
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        db_path_ = std::move(other.db_path_);
        other.db_ = nullptr;
        other.db_path_.clear();
    }
    return *this;
}

void Database::open(const std::string& path) {
    close();

    std::lock_guard<std::mutex> lock(mutex_);

    fprintf(stderr, "Database::open: path=%s\n", path.c_str());
    db_ = ::kv_open(path.c_str());
    fprintf(stderr, "Database::open: kv_open returned %p\n", (void*)db_);
    if (!db_) {
        throw DatabaseException("Failed to open KV database: " + path);
    }
    RAG_INFO("KV Database opened: " + path);
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        ::kv_flush(db_);
        ::kv_close(db_);
        db_ = nullptr;
        RAG_INFO("KV Database closed: " + db_path_);
    }
}

void Database::kv_put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) throw DatabaseException("Database not open");

    kv_result_t rc = ::kv_put(db_, key.data(), key.size(), value.data(), value.size());
    if (rc != KV_OK) {
        const char* errmsg = ::kv_errmsg(db_);
        throw DatabaseException("kv_put failed for key: " + key +
                               " (error=" + std::to_string((int)rc) +
                               ", msg=" + (errmsg ? errmsg : "null") + ")");
    }
}

std::optional<std::string> Database::kv_get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) throw DatabaseException("Database not open");

    void* out_value = nullptr;
    size_t out_len = 0;

    kv_result_t rc = ::kv_get(db_, key.data(), key.size(), &out_value, &out_len);
    if (rc == KV_NOT_FOUND) {
        return std::nullopt;
    }
    if (rc != KV_OK) {
        throw DatabaseException("kv_get failed for key: " + key);
    }

    std::string result(static_cast<const char*>(out_value), out_len);
    free(out_value);
    return result;
}

void Database::kv_delete(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) throw DatabaseException("Database not open");

    ::kv_delete(db_, key.data(), key.size());  // 忽略 NOT_FOUND
}

bool Database::kv_exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) throw DatabaseException("Database not open");

    return ::kv_exists(db_, key.data(), key.size());
}

std::vector<std::pair<std::string, std::string>> Database::kv_scan(
    const std::string& start_key, const std::string& end_key) {

    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) throw DatabaseException("Database not open");

    std::vector<std::pair<std::string, std::string>> results;

    kv_iter_t* iter = ::kv_scan(db_,
        start_key.data(), start_key.size(),
        end_key.data(), end_key.size());

    if (!iter) return results;

    while (::kv_iter_next(iter) == KV_OK) {
        const void* key = ::kv_iter_key(iter);
        size_t key_len = ::kv_iter_key_len(iter);
        const void* value = ::kv_iter_value(iter);
        size_t value_len = ::kv_iter_value_len(iter);

        results.emplace_back(
            std::string(static_cast<const char*>(key), key_len),
            std::string(static_cast<const char*>(value), value_len)
        );
    }

    ::kv_iter_free(iter);
    return results;
}

void Database::kv_flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        ::kv_flush(db_);
    }
}

// ========== DocumentRepository 实现 ==========

DocumentRepository::DocumentRepository(Database& db) : db_(db) {}

std::string DocumentRepository::serialize(const Document& doc) {
    json j;
    j["id"] = doc.id;
    j["content"] = doc.content;
    j["file_path"] = doc.metadata.file_path;
    j["file_name"] = doc.metadata.file_name;
    j["file_type"] = doc.metadata.file_type;
    j["file_size"] = doc.metadata.file_size;
    j["title"] = doc.metadata.title;
    j["author"] = doc.metadata.author;
    j["created_time"] = doc.metadata.created_time;
    j["modified_time"] = doc.metadata.modified_time;
    j["created_at"] = doc.created_at;
    j["updated_at"] = doc.updated_at;
    j["indexed_at"] = doc.indexed_at;
    j["status"] = static_cast<int>(doc.status);
    return j.dump();
}

Document DocumentRepository::deserialize(const std::string& data) {
    auto j = json::parse(data);
    Document doc;
    doc.id = j["id"].get<std::string>();
    doc.content = j.value("content", "");
    doc.metadata.file_path = j.value("file_path", "");
    doc.metadata.file_name = j.value("file_name", "");
    doc.metadata.file_type = j.value("file_type", "");
    doc.metadata.file_size = j.value("file_size", 0);
    doc.metadata.title = j.value("title", "");
    doc.metadata.author = j.value("author", "");
    doc.metadata.created_time = j.value("created_time", 0);
    doc.metadata.modified_time = j.value("modified_time", 0);
    doc.created_at = j.value("created_at", 0);
    doc.updated_at = j.value("updated_at", 0);
    doc.indexed_at = j.value("indexed_at", 0);
    doc.status = static_cast<Document::Status>(j.value("status", 0));
    return doc;
}

void DocumentRepository::insert(const Document& doc) {
    std::string key = std::string(DOC_PREFIX) + doc.id;
    std::string value = serialize(doc);
    db_.kv_put(key, value);

    // 维护反向索引：doc_path → doc_id
    if (!doc.metadata.file_path.empty()) {
        std::string path_key = std::string(DOC_PATH_PREFIX) + doc.metadata.file_path;
        db_.kv_put(path_key, doc.id);
    }
}

void DocumentRepository::update(const Document& doc) {
    insert(doc);  // KV put 是 upsert
}

void DocumentRepository::remove(const std::string& id) {
    // 先查找文档以获取 file_path（用于删除反向索引）
    auto doc = find_by_id(id);

    std::string key = std::string(DOC_PREFIX) + id;
    db_.kv_delete(key);

    // 删除反向索引
    if (doc && !doc->metadata.file_path.empty()) {
        std::string path_key = std::string(DOC_PATH_PREFIX) + doc->metadata.file_path;
        db_.kv_delete(path_key);
    }
}

std::optional<Document> DocumentRepository::find_by_id(const std::string& id) {
    std::string key = std::string(DOC_PREFIX) + id;
    auto value = db_.kv_get(key);
    if (!value) return std::nullopt;
    return deserialize(*value);
}

std::optional<Document> DocumentRepository::find_by_path(const std::string& path) {
    std::string path_key = std::string(DOC_PATH_PREFIX) + path;
    auto doc_id = db_.kv_get(path_key);
    if (!doc_id) return std::nullopt;
    return find_by_id(*doc_id);
}

std::vector<Document> DocumentRepository::find_all() {
    std::vector<Document> results;
    auto pairs = db_.kv_scan(
        std::string(DOC_PREFIX),
        std::string(DOC_PREFIX) + "\xFF"
    );
    for (const auto& [key, value] : pairs) {
        results.push_back(deserialize(value));
    }
    return results;
}

std::vector<Document> DocumentRepository::find_pending(int limit) {
    std::vector<Document> results;
    auto pairs = db_.kv_scan(
        std::string(DOC_PREFIX),
        std::string(DOC_PREFIX) + "\xFF"
    );
    for (const auto& [key, value] : pairs) {
        auto doc = deserialize(value);
        if (doc.status == Document::Status::PENDING) {
            results.push_back(std::move(doc));
            if (static_cast<int>(results.size()) >= limit) break;
        }
    }
    return results;
}

int64_t DocumentRepository::count() {
    auto pairs = db_.kv_scan(
        std::string(DOC_PREFIX),
        std::string(DOC_PREFIX) + "\xFF"
    );
    return static_cast<int64_t>(pairs.size());
}

int64_t DocumentRepository::count_by_status(Document::Status status) {
    int64_t cnt = 0;
    auto pairs = db_.kv_scan(
        std::string(DOC_PREFIX),
        std::string(DOC_PREFIX) + "\xFF"
    );
    for (const auto& [key, value] : pairs) {
        auto doc = deserialize(value);
        if (doc.status == status) cnt++;
    }
    return cnt;
}

// ========== ChunkRepository 实现 ==========

ChunkRepository::ChunkRepository(Database& db) : db_(db) {}

std::string ChunkRepository::serialize(const Chunk& chunk) {
    json j;
    j["id"] = chunk.id;
    j["document_id"] = chunk.document_id;
    j["content"] = chunk.content;
    j["chunk_index"] = chunk.chunk_index;
    j["start_char"] = chunk.start_char;
    j["end_char"] = chunk.end_char;
    j["start_line"] = chunk.start_line;
    j["end_line"] = chunk.end_line;
    j["num_tokens"] = chunk.num_tokens;
    j["file_path"] = chunk.metadata.file_path;
    j["file_name"] = chunk.metadata.file_name;
    j["file_type"] = chunk.metadata.file_type;
    return j.dump();
}

Chunk ChunkRepository::deserialize(const std::string& data) {
    auto j = json::parse(data);
    Chunk chunk;
    chunk.id = j["id"].get<std::string>();
    chunk.document_id = j["document_id"].get<std::string>();
    chunk.content = j["content"].get<std::string>();
    chunk.chunk_index = j.value("chunk_index", 0);
    chunk.start_char = j.value("start_char", 0);
    chunk.end_char = j.value("end_char", 0);
    chunk.start_line = j.value("start_line", 0);
    chunk.end_line = j.value("end_line", 0);
    chunk.num_tokens = j.value("num_tokens", 0);
    chunk.metadata.file_path = j.value("file_path", "");
    chunk.metadata.file_name = j.value("file_name", "");
    chunk.metadata.file_type = j.value("file_type", "");
    return chunk;
}

void ChunkRepository::insert(const Chunk& chunk) {
    std::string key = std::string(CHUNK_PREFIX) + chunk.id;
    std::string value = serialize(chunk);
    db_.kv_put(key, value);

    // 维护反向索引：chunk_doc:{doc_id} → chunk_id 列表
    // 使用追加方式存储多个 chunk_id
    std::string doc_key = std::string(CHUNK_DOC_PREFIX) + chunk.document_id;
    auto existing = db_.kv_get(doc_key);
    std::string ids;
    if (existing) {
        ids = *existing;
        if (!ids.empty()) ids += ",";
    }
    ids += chunk.id;
    db_.kv_put(doc_key, ids);
}

void ChunkRepository::insert_batch(const std::vector<Chunk>& chunks) {
    for (const auto& chunk : chunks) {
        insert(chunk);
    }
}

void ChunkRepository::remove(const std::string& id) {
    std::string key = std::string(CHUNK_PREFIX) + id;
    db_.kv_delete(key);
}

void ChunkRepository::remove_by_document(const std::string& document_id) {
    // 查找该文档的所有 chunk
    auto chunks = find_by_document(document_id);
    for (const auto& chunk : chunks) {
        remove(chunk.id);
    }
    // 删除反向索引
    std::string doc_key = std::string(CHUNK_DOC_PREFIX) + document_id;
    db_.kv_delete(doc_key);
}

std::optional<Chunk> ChunkRepository::find_by_id(const std::string& id) {
    std::string key = std::string(CHUNK_PREFIX) + id;
    auto value = db_.kv_get(key);
    if (!value) return std::nullopt;
    return deserialize(*value);
}

std::vector<Chunk> ChunkRepository::find_by_document(const std::string& document_id) {
    std::vector<Chunk> results;

    // 通过反向索引获取 chunk_id 列表
    std::string doc_key = std::string(CHUNK_DOC_PREFIX) + document_id;
    auto ids_value = db_.kv_get(doc_key);
    if (!ids_value) return results;

    // 解析逗号分隔的 ID 列表
    std::istringstream iss(*ids_value);
    std::string chunk_id;
    while (std::getline(iss, chunk_id, ',')) {
        if (!chunk_id.empty()) {
            auto chunk = find_by_id(chunk_id);
            if (chunk) {
                results.push_back(std::move(*chunk));
            }
        }
    }

    // 按 chunk_index 排序
    std::sort(results.begin(), results.end(),
        [](const Chunk& a, const Chunk& b) {
            return a.chunk_index < b.chunk_index;
        });

    return results;
}

int64_t ChunkRepository::count() {
    auto pairs = db_.kv_scan(
        std::string(CHUNK_PREFIX),
        std::string(CHUNK_PREFIX) + "\xFF"
    );
    return static_cast<int64_t>(pairs.size());
}

int64_t ChunkRepository::count_by_document(const std::string& document_id) {
    auto chunks = find_by_document(document_id);
    return static_cast<int64_t>(chunks.size());
}

// ========== IndexStatusRepository 实现 ==========

IndexStatusRepository::IndexStatusRepository(Database& db) : db_(db) {}

std::string IndexStatusRepository::serialize(const IndexStatus& status) {
    json j;
    j["index_name"] = status.index_name;
    j["document_count"] = status.document_count;
    j["chunk_count"] = status.chunk_count;
    j["vector_count"] = status.vector_count;
    j["last_update"] = status.last_update;
    j["index_size"] = status.index_size;
    j["status"] = static_cast<int>(status.status);
    return j.dump();
}

IndexStatus IndexStatusRepository::deserialize(const std::string& data) {
    auto j = json::parse(data);
    IndexStatus status;
    status.index_name = j["index_name"].get<std::string>();
    status.document_count = j.value("document_count", 0);
    status.chunk_count = j.value("chunk_count", 0);
    status.vector_count = j.value("vector_count", 0);
    status.last_update = j.value("last_update", 0);
    status.index_size = j.value("index_size", 0);
    status.status = static_cast<IndexStatus::Status>(j.value("status", 0));
    return status;
}

std::optional<IndexStatus> IndexStatusRepository::get(const std::string& name) {
    std::string key = std::string(STATUS_PREFIX) + name;
    auto value = db_.kv_get(key);
    if (!value) return std::nullopt;
    return deserialize(*value);
}

void IndexStatusRepository::update(const IndexStatus& status) {
    upsert(status);
}

void IndexStatusRepository::upsert(const IndexStatus& status) {
    std::string key = std::string(STATUS_PREFIX) + status.index_name;
    std::string value = serialize(status);
    db_.kv_put(key, value);
}

}  // namespace mmrag
