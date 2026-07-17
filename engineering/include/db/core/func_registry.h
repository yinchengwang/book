/**
 * @file func_registry.h
 * @brief SQL 函数注册表单一头文件
 *
 * 统一注册所有模型的函数（向量、时序、图、文档、空间、KV、树）
 */
#ifndef DB_FUNC_REGISTRY_H
#define DB_FUNC_REGISTRY_H

#include "db/storage_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 函数类型
 * ======================================================================== */

/** 函数类别 */
typedef enum FunctionClass_e {
    FUNC_CLASS_NORMAL = 0,     /**< 普通函数 */
    FUNC_CLASS_AGGREGATE,       /**< 聚合函数 */
    FUNC_CLASS_WINDOW,         /**< 窗口函数 */
    FUNC_CLASS_PROCEDURAL,      /**< 过程函数 */
    FUNC_CLASS_TRIGGER,         /**< 触发器函数 */
} FunctionClass;

/** 函数返回类型 */
typedef enum FunctionResultType_e {
    FUNC_RESULT_SCALAR = 0,    /**< 标量返回值 */
    FUNC_RESULT_SET,            /**< 集合返回值 */
    FUNC_RESULT_TABLE,          /**< 表返回值 */
} FunctionResultType;

/* ========================================================================
 * 函数参数
 * ======================================================================== */

/** 参数模式 */
typedef enum FuncArgMode_e {
    ARG_MODE_IN = 0,           /**< 输入参数 */
    ARG_MODE_OUT,               /**< 输出参数 */
    ARG_MODE_INOUT,             /**< 输入输出 */
    ARG_MODE_VARARG,            /**< 可变参数 */
} FuncArgMode;

/** 参数信息 */
typedef struct FunctionArg_s {
    const char *name;           /**< 参数名 */
    int type_oid;               /**< 参数类型 OID */
    FuncArgMode mode;           /**< 参数模式 */
    const char *default_val;    /**< 默认值 */
} FunctionArg;

/* ========================================================================
 * 函数信息
 * ======================================================================== */

/** 函数信息 */
typedef struct FunctionInfo_s {
    const char *name;           /**< 函数名 */
    const char *schema;         /**< 模式名 */
    FunctionClass func_class;    /**< 函数类别 */
    FunctionResultType result_type; /**< 返回类型 */
    int num_args;               /**< 参数数量 */
    FunctionArg *args;          /**< 参数数组 */
    int vararg_num;             /**< 可变参数数量 */
    int result_oid;             /**< 返回类型 OID */
    void *func_impl;            /**< 函数实现 */
    void (*func_ptr)(void);     /**< 函数指针 */
    const char *description;    /**< 描述 */
    DataModel model;             /**< 所属模型 */
    bool is_variadic;          /**< 是否可变参数 */
} FunctionInfo;

/* ========================================================================
 * 函数注册表
 * ======================================================================== */

/**
 * @brief 初始化函数注册表
 */
int func_registry_init(void);

/**
 * @brief 关闭函数注册表
 */
void func_registry_shutdown(void);

/**
 * @brief 注册函数
 * @param info 函数信息
 * @return 0 成功，-1 失败
 */
int func_registry_register(const FunctionInfo *info);

/**
 * @brief 注销函数
 * @param name 函数名
 * @param schema 模式名
 * @return 0 成功，-1 未找到
 */
int func_registry_unregister(const char *name, const char *schema);

/**
 * @brief 查找函数
 * @param name 函数名
 * @param schema 模式名
 * @param num_args 参数数量（-1 表示任意）
 * @return 函数信息，NULL 未找到
 */
const FunctionInfo *func_registry_lookup(const char *name, const char *schema, int num_args);

/**
 * @brief 按模型列出函数
 * @param model 数据模型
 * @param out_count 输出函数数量
 * @return 函数数组
 */
const FunctionInfo **func_registry_list_by_model(DataModel model, size_t *out_count);

/**
 * @brief 按类别列出函数
 */
const FunctionInfo **func_registry_list_by_class(FunctionClass func_class, size_t *out_count);

/**
 * @brief 获取函数数量
 */
size_t func_registry_count(void);

/**
 * @brief 验证函数参数
 * @return true 有效，false 无效
 */
bool func_registry_validate_args(const FunctionInfo *func, const int *arg_types, int num_args);

/* ========================================================================
 * 内置函数注册
 * ======================================================================== */

/**
 * @brief 注册所有内置函数
 */
int func_registry_register_builtins(void);

/**
 * @brief 注册向量函数
 */
int func_registry_register_vector(void);

/**
 * @brief 注册时序函数
 */
int func_registry_register_timeseries(void);

/**
 * @brief 注册图函数
 */
int func_registry_register_graph(void);

/**
 * @brief 注册文档函数
 */
int func_registry_register_document(void);

/**
 * @brief 注册空间函数
 */
int func_registry_register_spatial(void);

/**
 * @brief 注册 KV 函数
 */
int func_registry_register_kv(void);

/**
 * @brief 注册树函数
 */
int func_registry_register_tree(void);

/* ========================================================================
 * 函数执行器
 * ======================================================================== */

/** 函数执行上下文 */
typedef struct FuncExecutor_s {
    const FunctionInfo *func;  /**< 函数信息 */
    void **args;               /**< 参数值 */
    int num_args;               /**< 参数数量 */
    void *result;               /**< 结果 */
    void *extra;                /**< 额外数据 */
} FuncExecutor;

/**
 * @brief 创建函数执行器
 */
FuncExecutor *func_executor_create(const FunctionInfo *func);

/**
 * @brief 设置参数
 */
int func_executor_set_arg(FuncExecutor *exec, int arg_idx, void *value);

/**
 * @brief 执行函数
 */
int func_executor_execute(FuncExecutor *exec);

/**
 * @brief 获取结果
 */
void *func_executor_get_result(FuncExecutor *exec);

/**
 * @brief 销毁执行器
 */
void func_executor_destroy(FuncExecutor *exec);

/* ========================================================================
 * 类型系统
 * ======================================================================== */

/** 类型信息 */
typedef struct TypeInfo_s {
    const char *name;           /**< 类型名 */
    int oid;                   /**< 类型 OID */
    int len;                   /**< 长度（-1 可变） */
    bool byval;                /**< 是否按值传递 */
    void (*recv)(void);        /**< 接收函数 */
    void (*send)(void);        /**< 发送函数 */
} TypeInfo;

/**
 * @brief 注册类型
 */
int func_registry_register_type(const TypeInfo *info);

/**
 * @brief 查找类型
 */
const TypeInfo *func_registry_lookup_type(int oid);

/**
 * @brief 获取类型 OID
 */
int func_registry_get_type_oid(const char *name);

/**
 * @brief 初始化类型系统
 */
int func_registry_init_types(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_FUNC_REGISTRY_H */
