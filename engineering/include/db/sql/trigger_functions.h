/**
 * @file trigger_functions.h
 * @brief SQL 触发器函数注册与调用机制
 *
 * 实现触发器函数的注册表管理和调用机制：
 *   - 触发器函数注册表
 *   - 内置触发器函数（auto_timestamp、audit_log、validate_data）
 *   - 自定义触发器函数注册接口
 *
 * 本文件是 SQL 执行引擎 Task 5.5 触发器调用机制实现。
 */

#ifndef DB_SQL_TRIGGER_FUNCTIONS_H
#define DB_SQL_TRIGGER_FUNCTIONS_H

#include <stdint.h>
#include <stdbool.h>

#include "db/sql/nodes/nodetags.h"
#include "db/sql/trigger.h"
#include "db/sql/memctx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct TriggerData       TriggerData;
typedef struct TupleTableSlot    TupleTableSlot;
typedef struct EState            EState;
typedef struct TriggerDesc       TriggerDesc;

/* ========================================================================
 * Oid 类型定义
 * ======================================================================== */

#ifndef Oid_defined
#define Oid_defined
typedef uint64_t Oid;    /**< 对象标识符 */
#endif

/* ========================================================================
 * 触发器函数类型定义
 * ======================================================================== */

/**
 * @brief 触发器函数指针类型
 *
 * 触发器函数的签名必须为：
 *   TupleTableSlot *func(TriggerData *trig_data)
 *
 * BEFORE 触发器可以修改元组并返回新的 TupleTableSlot。
 * AFTER 触发器通常返回 NULL（不修改元组）。
 */
typedef TupleTableSlot *(*TriggerFunc)(TriggerData *trig_data);

/* ========================================================================
 * 内置触发器函数 OID
 * ======================================================================== */

/**
 * @brief 内置触发器函数 OID 定义
 */
#define TRIGGER_FUNC_AUTO_TIMESTAMP   ((Oid)1000)  /**< 自动时间戳 */
#define TRIGGER_FUNC_AUDIT_LOG        ((Oid)1001)  /**< 审计日志 */
#define TRIGGER_FUNC_VALIDATE_DATA    ((Oid)1002)  /**< 数据验证 */

/* ========================================================================
 * 触发器函数条目
 * ======================================================================== */

/**
 * @brief TriggerFunctionEntry - 触发器函数条目
 *
 * 存储在触发器函数注册表中的条目。
 */
typedef struct TriggerFunctionEntry {
    char           *func_name;      /**< 函数名称 */
    Oid             func_oid;       /**< 函数 OID */
    TriggerFunc     func_ptr;       /**< 函数指针 */
    bool            is_builtin;     /**< 是否为内置函数 */
    struct TriggerFunctionEntry *next;  /**< 链表下一项 */
} TriggerFunctionEntry;

/* ========================================================================
 * 触发器函数注册表
 * ======================================================================== */

/**
 * @brief 初始化触发器函数注册表
 *
 * 注册所有内置触发器函数：
 *   - auto_timestamp: BEFORE INSERT 自动设置 created_at
 *   - audit_log: AFTER 操作记录审计日志
 *   - validate_data: BEFORE 验证数据完整性
 *
 * @return 0 成功；非 0 失败
 */
int InitTriggerFunctionRegistry(void);

/**
 * @brief 销毁触发器函数注册表
 *
 * 释放所有注册的触发器函数条目。
 */
void DestroyTriggerFunctionRegistry(void);

/**
 * @brief 注册触发器函数
 *
 * @param name     函数名称（将被复制）
 * @param oid      函数 OID（唯一标识）
 * @param func     函数指针
 * @param builtin  是否为内置函数
 *
 * @return 0 成功；-1 参数错误；-2 OID 已存在
 */
int RegisterTriggerFunction(const char *name, Oid oid, TriggerFunc func, bool builtin);

/**
 * @brief 查找触发器函数
 *
 * @param oid 函数 OID
 *
 * @return 触发器函数指针；未找到返回 NULL
 */
TriggerFunc FindTriggerFunction(Oid oid);

/**
 * @brief 按名称查找触发器函数
 *
 * @param name 函数名称
 *
 * @return 触发器函数指针；未找到返回 NULL
 */
TriggerFunc FindTriggerFunctionByName(const char *name);

/**
 * @brief 获取触发器函数信息
 *
 * @param oid 函数 OID
 *
 * @return 函数名称；未找到返回 NULL
 */
const char *GetTriggerFunctionName(Oid oid);

/**
 * @brief 检查触发器函数是否存在
 *
 * @param oid 函数 OID
 *
 * @return true 存在；false 不存在
 */
bool TriggerFunctionExists(Oid oid);

/**
 * @brief 获取注册表大小
 *
 * @return 已注册的函数数量
 */
int GetTriggerFunctionCount(void);

/* ========================================================================
 * 内置触发器函数声明
 * ======================================================================== */

/**
 * @brief auto_timestamp - 自动时间戳触发器
 *
 * BEFORE INSERT 触发器，自动设置 created_at 字段。
 * 假设表中有 created_at 字段（第一个 timestamp 类型字段）。
 *
 * @param trig_data 触发器执行上下文
 *
 * @return 修改后的元组槽；NULL 表示跳过操作
 */
TupleTableSlot *trigger_auto_timestamp(TriggerData *trig_data);

/**
 * @brief audit_log - 审计日志触发器
 *
 * AFTER 触发器，记录操作到审计日志。
 * 当前实现为占位，将操作信息打印到 stderr。
 *
 * @param trig_data 触发器执行上下文
 *
 * @return NULL（AFTER 触发器不修改元组）
 */
TupleTableSlot *trigger_audit_log(TriggerData *trig_data);

/**
 * @brief validate_data - 数据验证触发器
 *
 * BEFORE 触发器，验证数据完整性。
 * 当前实现为占位，检查必填字段非空。
 *
 * @param trig_data 触发器执行上下文
 *
 * @return 原元组槽（验证通过）或 NULL（验证失败）
 */
TupleTableSlot *trigger_validate_data(TriggerData *trig_data);

/* ========================================================================
 * 触发器调用封装
 * ======================================================================== */

/**
 * @brief 执行触发器函数
 *
 * 查找并执行指定的触发器函数。
 *
 * @param trig_data 触发器执行上下文
 *
 * @return 触发器函数返回的元组槽；NULL 表示函数未注册或执行失败
 */
TupleTableSlot *CallTriggerFunction(TriggerData *trig_data);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_TRIGGER_FUNCTIONS_H */
