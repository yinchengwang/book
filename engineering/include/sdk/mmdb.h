/**
 * @file mmdb.h
 * @brief 多模态 SDK 顶层入口
 */
#ifndef SDK_MMDB_H
#define SDK_MMDB_H

#include "sdk/mmdb_types.h"
#include "sdk/mmdb_error.h"
#include "sdk/impl/mmdb_memctx.h"  /* Task 9：暴露 MemoryContext 给请求作用域 API */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 生命周期
 * ======================================================================== */

mmdb_t* mmdb_open(const char* path, const mmdb_options_t* opts);
void    mmdb_close(mmdb_t* db);
int     mmdb_last_error_code(mmdb_t* db);
const char* mmdb_last_error_message(mmdb_t* db);

/* ========================================================================
 * Collection 管理
 * ======================================================================== */

mmdb_collection_t* mmdb_collection_get(mmdb_t* db, const char* name);
mmdb_collection_t* mmdb_collection_create(mmdb_t* db, const char* name,
                                          const mmdb_schema_t* schema);
void               mmdb_collection_drop(mmdb_collection_t* coll);
const char*        mmdb_collection_name(mmdb_collection_t* coll);
mmdb_t*            mmdb_collection_db(mmdb_collection_t* coll);

/* ========================================================================
 * 结果释放
 * ======================================================================== */

void mmdb_result_free(mmdb_result_t* result);
void mmdb_path_free(mmdb_path_t* path);

/* ========================================================================
 * 请求级内存作用域（Task 9）
 *
 * 用法示例：
 *   mmdb_request_scope_t scope;
 *   if (mmdb_request_begin(db, "my-op", &scope) != MMDB_OK) return NULL;
 *   // 所有分配在 scope.context 中进行
 *   mmdb_request_end(&scope);
 * ======================================================================== */

/**
 * @brief 请求作用域结构
 *
 * 封装单个请求的内存上下文生命周期。
 * 通过 mmdb_request_begin 创建，mmdb_request_end 销毁。
 */
typedef struct mmdb_request_scope {
    mmdb_t*        db;         /**< 数据库句柄 */
    MemoryContext  context;    /**< 当前请求上下文 */
    MemoryContext  previous;   /**< 请求前的旧上下文 */
    int            active;     /**< 是否活跃（begin 后为 1，end 后为 0） */
} mmdb_request_scope_t;

/**
 * @brief 开始请求级内存作用域
 *
 * 在 connection_context 下创建一个新的请求上下文，并切换为当前上下文。
 * 后续所有 palloc/pfree 操作将在此请求上下文中进行。
 *
 * @param db    数据库句柄（不可为 NULL）
 * @param name  上下文名称（调试用）
 * @param scope 输出参数，存储作用域状态（不可为 NULL）
 *
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 参数无效；MMDB_ERR_NOMEM 内存不足
 */
int mmdb_request_begin(mmdb_t* db, const char* name, mmdb_request_scope_t* scope);

/**
 * @brief 结束请求级内存作用域
 *
 * 恢复之前的内存上下文，销毁请求上下文及其所有子上下文。
 * 请求上下文中分配的所有内存自动释放。
 *
 * @param scope 请求作用域（不可为 NULL，非活跃状态安全返回）
 */
void mmdb_request_end(mmdb_request_scope_t* scope);

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_H */
