/**
 * @file database.h
 * @brief 数据库 - 自研多模态 KV 存储引擎
 *
 * 使用自研 KV 引擎（kv_t）替代 SQLite，通过 key 前缀命名空间实现表隔离：
 *   doc:{id}           → Document JSON
 *   doc_path:{path}    → document ID（反向索引）
 *   chunk:{id}         → Chunk JSON
 *   chunk_doc:{doc_id} → chunk ID 列表（反向索引）
 *   status:{name}      → IndexStatus JSON
 */
#pragma once

#include "mmrag/types.h"
#include "mmrag/error.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <mutex>

struct kv_s;
typedef struct kv_s kv_t;

namespace mmrag {

// ========== 前向声明 ==========

class Database;

// ========== 数据库异常 ==========

class DatabaseException : public RAGException {
public:
    explicit DatabaseException(const std::string& message)
        : RAGException(errors::DATABASE_ERROR, message, "Database") {}
};

// ========== 数据库类 ==========

/**
 * @brief 自研 KV 存储引擎封装
 */
class Database {
public:
    Database();
    ~Database();

    // 禁止拷贝
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // 移动
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;

    // 打开数据库
    void open(const std::string& path);

    // 关闭数据库
    void close();

    // KV 操作（替代 SQL）
    void kv_put(const std::string& key, const std::string& value);
    std::optional<std::string> kv_get(const std::string& key);
    void kv_delete(const std::string& key);
    bool kv_exists(const std::string& key);

    // 范围扫描
    std::vector<std::pair<std::string, std::string>> kv_scan(
        const std::string& start_key, const std::string& end_key);

    // 刷盘
    void kv_flush();

    // 获取原始 kv_t 句柄（保留兼容性，不建议直接使用）
    kv_t* handle() const { return db_; }

    // 检查数据库是否打开
    bool is_open() const { return db_ != nullptr; }

    // 获取路径
    const std::string& path() const { return db_path_; }

private:
    kv_t* db_ = nullptr;
    std::string db_path_;
    mutable std::mutex mutex_;
};

// ========== Repository 接口 ==========

/**
 * @brief 文档 Repository
 */
class DocumentRepository {
public:
    explicit DocumentRepository(Database& db);

    // 插入
    void insert(const Document& doc);

    // 更新
    void update(const Document& doc);

    // 删除
    void remove(const std::string& id);

    // 按 ID 查询
    std::optional<Document> find_by_id(const std::string& id);

    // 按路径查询
    std::optional<Document> find_by_path(const std::string& path);

    // 查询所有
    std::vector<Document> find_all();

    // 查询待处理的文档
    std::vector<Document> find_pending(int limit = 100);

    // 统计
    int64_t count();
    int64_t count_by_status(Document::Status status);

private:
    Database& db_;

    // key 前缀
    static constexpr const char* DOC_PREFIX = "doc:";
    static constexpr const char* DOC_PATH_PREFIX = "doc_path:";

    // 序列化/反序列化
    std::string serialize(const Document& doc);
    Document deserialize(const std::string& data);
};

/**
 * @brief 块 Repository
 */
class ChunkRepository {
public:
    explicit ChunkRepository(Database& db);

    // 插入
    void insert(const Chunk& chunk);

    // 批量插入
    void insert_batch(const std::vector<Chunk>& chunks);

    // 删除
    void remove(const std::string& id);
    void remove_by_document(const std::string& document_id);

    // 按 ID 查询
    std::optional<Chunk> find_by_id(const std::string& id);

    // 按文档查询
    std::vector<Chunk> find_by_document(const std::string& document_id);

    // 统计
    int64_t count();
    int64_t count_by_document(const std::string& document_id);

private:
    Database& db_;

    // key 前缀
    static constexpr const char* CHUNK_PREFIX = "chunk:";
    static constexpr const char* CHUNK_DOC_PREFIX = "chunk_doc:";

    // 序列化/反序列化
    std::string serialize(const Chunk& chunk);
    Chunk deserialize(const std::string& data);
};

/**
 * @brief 索引状态 Repository
 */
class IndexStatusRepository {
public:
    explicit IndexStatusRepository(Database& db);

    // 获取状态
    std::optional<IndexStatus> get(const std::string& name);

    // 更新状态
    void update(const IndexStatus& status);

    // 创建或更新
    void upsert(const IndexStatus& status);

private:
    Database& db_;

    // key 前缀
    static constexpr const char* STATUS_PREFIX = "status:";

    // 序列化/反序列化
    std::string serialize(const IndexStatus& status);
    IndexStatus deserialize(const std::string& data);
};

}  // namespace mmrag
