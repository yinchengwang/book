/**
 * @file mmdb_sdk.h
 * @brief 多模态数据库 SDK 头文件
 *
 * 提供 Python SDK 和 JDBC Driver 的 C 接口封装：
 * 1. 连接管理
 * 2. SQL 执行
 * 3. 向量操作
 * 4. 类型映射
 */
#ifndef DB_MMDB_SDK_H
#define DB_MMDB_SDK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 连接管理
 * ======================================================================== */

/** 连接句柄 */
typedef struct MMDBConnection_s {
    int fd;                  /**< socket fd */
    char *host;             /**< 主机 */
    int port;               /**< 端口 */
    char *user;             /**< 用户名 */
    char *password;         /**< 密码 */
    char *database;         /**< 数据库 */
    bool connected;          /**< 是否已连接 */
    void *internal;         /**< 内部数据 */
} MMDBConnection;

/**
 * @brief 创建连接
 */
MMDBConnection *mmdb_connect(const char *host, int port,
                           const char *user, const char *password,
                           const char *database);

/**
 * @brief 关闭连接
 */
void mmdb_disconnect(MMDBConnection *conn);

/**
 * @brief 检查连接状态
 */
bool mmdb_is_connected(const MMDBConnection *conn);

/**
 * @brief 获取连接错误
 */
const char *mmdb_get_error(const MMDBConnection *conn);

/**
 * @brief 设置连接选项
 */
int mmdb_set_option(MMDBConnection *conn, const char *name, const char *value);

/**
 * @brief 获取连接选项
 */
const char *mmdb_get_option(const MMDBConnection *conn, const char *name);

/* ========================================================================
 * SQL 执行
 * ======================================================================== */

/** 结果集句柄 */
typedef struct MMDBCursor_s {
    MMDBConnection *conn;  /**< 连接 */
    void *result;          /**< 结果集 */
    int row_count;         /**< 行数 */
    int col_count;         /**< 列数 */
    int current_row;       /**< 当前行 */
    void *internal;        /**< 内部数据 */
} MMDBCursor;

/**
 * @brief 执行 SQL 查询
 *
 * @param conn 连接
 * @param sql SQL 语句
 * @return 游标（需手动释放）
 */
MMDBCursor *mmdb_execute(MMDBConnection *conn, const char *sql);

/**
 * @brief 执行 SQL 并获取影响行数
 *
 * @param conn 连接
 * @param sql SQL 语句
 * @return 影响行数，-1 表示错误
 */
int64_t mmdb_execute_update(MMDBConnection *conn, const char *sql);

/**
 * @brief 准备语句
 *
 * @param conn 连接
 * @param sql SQL 语句
 * @return 语句 ID
 */
int mmdb_prepare(MMDBConnection *conn, const char *sql);

/**
 * @brief 执行预处理语句
 *
 * @param conn 连接
 * @param stmt_id 语句 ID
 * @param params 参数数组
 * @param nparams 参数数量
 * @return 游标
 */
MMDBCursor *mmdb_execute_prepared(MMDBConnection *conn, int stmt_id,
                                  void **params, int nparams);

/**
 * @brief 释放游标
 */
void mmdb_cursor_close(MMDBCursor *cursor);

/**
 * @brief 获取列数
 */
int mmdb_get_column_count(const MMDBCursor *cursor);

/**
 * @brief 获取行数
 */
int64_t mmdb_get_row_count(const MMDBCursor *cursor);

/**
 * @brief 获取列名
 */
const char *mmdb_get_column_name(const MMDBCursor *cursor, int col);

/**
 * @brief 获取列类型
 */
int mmdb_get_column_type(const MMDBCursor *cursor, int col);

/**
 * @brief 获取列类型名
 */
const char *mmdb_get_column_type_name(const MMDBCursor *cursor, int col);

/**
 * @brief 下移游标
 */
bool mmdb_cursor_next(MMDBCursor *cursor);

/**
 * @brief 重置游标到开始
 */
void mmdb_cursor_reset(MMDBCursor *cursor);

/**
 * @brief 获取整数值
 */
int64_t mmdb_get_int(MMDBCursor *cursor, int col);

/**
 * @brief 获取 64 位整数值
 */
int64_t mmdb_get_int64(MMDBCursor *cursor, int col);

/**
 * @brief 获取双精度值
 */
double mmdb_get_double(MMDBCursor *cursor, int col);

/**
 * @brief 获取字符串值
 */
const char *mmdb_get_string(MMDBCursor *cursor, int col);

/**
 * @brief 获取二进制值
 */
const void *mmdb_get_bytes(MMDBCursor *cursor, int col, int *len);

/**
 * @brief 获取布尔值
 */
bool mmdb_get_bool(MMDBCursor *cursor, int col);

/**
 * @brief 检查值是否为 NULL
 */
bool mmdb_is_null(MMDBCursor *cursor, int col);

/* ========================================================================
 * 向量操作
 * ======================================================================== */

/** 向量 */
typedef struct MMDBVector_s {
    float *data;           /**< 数据 */
    int dim;               /**< 维度 */
    int size;              /**< 字节大小 */
} MMDBVector;

/**
 * @brief 创建向量
 */
MMDBVector *mmdb_vector_create(float *data, int dim);

/**
 * @brief 释放向量
 */
void mmdb_vector_destroy(MMDBVector *vec);

/**
 * @brief 复制向量
 */
MMDBVector *mmdb_vector_copy(const MMDBVector *vec);

/**
 * @brief 插入向量
 *
 * @param conn 连接
 * @param collection 集合名
 * @param vec 向量
 * @param metadata 元数据 JSON
 * @return 向量 ID
 */
int64_t mmdb_vector_insert(MMDBConnection *conn, const char *collection,
                          const MMDBVector *vec, const char *metadata);

/**
 * @brief 批量插入向量
 *
 * @param conn 连接
 * @param collection 集合名
 * @param vecs 向量数组
 * @param nvecs 向量数量
 * @return 插入数量
 */
int64_t mmdb_vector_insert_batch(MMDBConnection *conn, const char *collection,
                                MMDBVector **vecs, int nvecs);

/**
 * @brief 搜索最近邻
 *
 * @param conn 连接
 * @param collection 集合名
 * @param query 查询向量
 * @param top_k 返回数量
 * @return 搜索结果游标
 */
MMDBCursor *mmdb_vector_search(MMDBConnection *conn, const char *collection,
                               const MMDBVector *query, int top_k);

/**
 * @brief 带过滤的向量搜索
 *
 * @param conn 连接
 * @param collection 集合名
 * @param query 查询向量
 * @param top_k 返回数量
 * @param filter 过滤条件 JSON
 * @return 搜索结果游标
 */
MMDBCursor *mmdb_vector_search_filter(MMDBConnection *conn,
                                      const char *collection,
                                      const MMDBVector *query,
                                      int top_k,
                                      const char *filter);

/**
 * @brief 删除向量
 *
 * @param conn 连接
 * @param collection 集合名
 * @param ids 向量 ID 数组
 * @param nids ID 数量
 * @return 删除数量
 */
int64_t mmdb_vector_delete(MMDBConnection *conn, const char *collection,
                          int64_t *ids, int nids);

/**
 * @brief 获取向量
 */
MMDBCursor *mmdb_vector_get(MMDBConnection *conn, const char *collection,
                           int64_t id);

/**
 * @brief 更新向量
 */
int mmdb_vector_update(MMDBConnection *conn, const char *collection,
                      int64_t id, const MMDBVector *vec);

/* ========================================================================
 * 集合管理
 * ======================================================================== */

/** 集合信息 */
typedef struct MMDBCollectionInfo_s {
    char name[128];        /**< 名称 */
    int vector_dim;       /**< 向量维度 */
    int64_t count;        /**< 向量数量 */
    int64_t size_bytes;   /**< 大小 */
} MMDBCollectionInfo;

/**
 * @brief 创建集合
 *
 * @param conn 连接
 * @param name 集合名
 * @param dim 向量维度
 * @param config 配置 JSON
 * @return 0 成功
 */
int mmdb_collection_create(MMDBConnection *conn, const char *name,
                           int dim, const char *config);

/**
 * @brief 删除集合
 */
int mmdb_collection_drop(MMDBConnection *conn, const char *name);

/**
 * @brief 获取集合信息
 */
MMDBCollectionInfo *mmdb_collection_info(MMDBConnection *conn,
                                       const char *name);

/**
 * @brief 获取集合列表
 */
MMDBCursor *mmdb_collection_list(MMDBConnection *conn);

/**
 * @brief 释放集合信息
 */
void mmdb_collection_info_destroy(MMDBCollectionInfo *info);

/* ========================================================================
 * 事务管理
 * ======================================================================== */

/**
 * @brief 开始事务
 */
int mmdb_begin(MMDBConnection *conn);

/**
 * @brief 提交事务
 */
int mmdb_commit(MMDBConnection *conn);

/**
 * @brief 回滚事务
 */
int mmdb_rollback(MMDBConnection *conn);

/**
 * @brief 设置保存点
 */
int mmdb_savepoint(MMDBConnection *conn, const char *name);

/**
 * @brief 回滚到保存点
 */
int mmdb_rollback_to(MMDBConnection *conn, const char *name);

/**
 * @brief 释放保存点
 */
int mmdb_release_savepoint(MMDBConnection *conn, const char *name);

/* ========================================================================
 * 连接池（高级）
 * ======================================================================== */

/** 连接池句柄 */
typedef struct MMDBPool_s {
    char *host;
    int port;
    char *user;
    char *password;
    char *database;
    int max_size;         /**< 最大连接数 */
    int min_size;         /**< 最小连接数 */
    void *internal;
} MMDBPool;

/**
 * @brief 创建连接池
 */
MMDBPool *mmdb_pool_create(const char *host, int port,
                           const char *user, const char *password,
                           const char *database, int max_size);

/**
 * @brief 销毁连接池
 */
void mmdb_pool_destroy(MMDBPool *pool);

/**
 * @brief 获取连接
 */
MMDBConnection *mmdb_pool_get_connection(MMDBPool *pool);

/**
 * @brief 释放连接回池
 */
void mmdb_pool_release(MMDBPool *pool, MMDBConnection *conn);

/**
 * @brief 刷新连接池
 */
void mmdb_pool_refresh(MMDBPool *pool);

/* ========================================================================
 * 类型映射
 * ======================================================================== */

/** MMDB 类型到 C 类型映射 */
typedef enum MMDBType_e {
    MMDB_NULL = 0,
    MMDB_BOOL = 1,
    MMDB_INT = 2,
    MMDB_INT64 = 3,
    MMDB_FLOAT = 4,
    MMDB_DOUBLE = 5,
    MMDB_STRING = 6,
    MMDB_BYTES = 7,
    MMDB_VECTOR = 8,
    MMDB_TIMESTAMP = 9,
    MMDB_DATE = 10,
    MMDB_JSON = 11,
    MMDB_ARRAY = 12,
    MMDB_OBJECT = 13
} MMDBType;

/**
 * @brief 获取类型名称
 */
const char *mmdb_type_name(MMDBType type);

/**
 * @brief 获取类型代码
 */
MMDBType mmdb_type_from_name(const char *name);

/**
 * @brief 安全释放内存
 */
void mmdb_free(void *ptr);

/* ========================================================================
 * 错误处理
 * ======================================================================== */

/** 错误码 */
typedef enum MMDBErrorCode_e {
    MMDB_OK = 0,
    MMDB_ERROR_CONNECTION = 1,
    MMDB_ERROR_TIMEOUT = 2,
    MMDB_ERROR_SQL = 3,
    MMDB_ERROR_INVALID_PARAM = 4,
    MMDB_ERROR_NO_DATA = 5,
    MMDB_ERROR_DUPLICATE_KEY = 6,
    MMDB_ERROR_CONSTRAINT = 7,
    MMDB_ERROR_UNKNOWN = 99
} MMDBErrorCode;

/**
 * @brief 获取错误码
 */
MMDBErrorCode mmdb_error_code(const MMDBConnection *conn);

/**
 * @brief 获取错误消息
 */
const char *mmdb_error_message(const MMDBConnection *conn);

/**
 * @brief 获取 SQL 状态码
 */
const char *mmdb_sqlstate(const MMDBConnection *conn);

#ifdef __cplusplus
}
#endif

#endif /* DB_MMDB_SDK_H */
