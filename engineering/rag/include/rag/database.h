/**
 * @file database.h
 * @brief 数据库 - 无外部依赖版本
 */
#pragma once

#include "rag/types.h"
#include "rag/error.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace rag {

// ========== 前向声明 ==========

class Database;
class Transaction;

// ========== 数据库异常 ==========

class DatabaseException : public RAGException {
public:
    using RAGException::RAGException;
};

// ========== 数据库类 ==========

/**
 * @brief 数据库封装（无外部依赖版本）
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

    // 执行 SQL
    void execute(const std::string& sql);

    // 执行带参数的 SQL
    void execute(const std::string& sql, const std::vector<std::string>& params);

    // 查询
    std::vector<std::map<std::string, std::string>> query(
        const std::string& sql,
        const std::vector<std::string>& params = {});

    // 准备语句（stub）
    struct Stmt {};
    using StmtPtr = std::unique_ptr<Stmt, void(*)(Stmt*)>;
    StmtPtr prepare(const std::string& sql);

    // 事务
    Transaction begin_transaction();

    // 检查数据库是否打开
    bool is_open() const { return !db_path_.empty(); }

    // 获取路径
    const std::string& path() const { return db_path_; }

private:
    std::string db_path_;
};

// ========== 事务 ==========

/**
 * @brief 数据库事务
 */
class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();

    // 禁止拷贝
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    // 移动
    Transaction(Transaction&&) noexcept;
    Transaction& operator=(Transaction&&) noexcept;

    // 提交
    void commit();

    // 回滚
    void rollback();

    // 是否已提交/回滚
    bool is_active() const { return active_; }

private:
    Database& db_;
    bool active_ = true;
};

// ========== Schema 管理 ==========

/**
 * @brief 数据库 Schema 管理器
 */
class SchemaManager {
public:
    explicit SchemaManager(Database& db);

    // 初始化 Schema
    void init();

    // 检查 Schema 是否存在
    bool exists();

    // 获取版本
    int get_version();

    // 设置版本
    void set_version(int version);

private:
    Database& db_;

    // Schema 版本
    static constexpr int CURRENT_VERSION = 1;
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
};

}  // namespace rag
