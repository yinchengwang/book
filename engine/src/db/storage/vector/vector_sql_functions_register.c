/**
 * @file vector_sql_functions_register.c
 * @brief 向量模态内置 SQL 函数注册（Task 3 自 func_registry.c 迁入）
 *
 * mm_builtin_engines.c 经 func_registry_register_table 下行注册（模态零 L1 调用），
 * 模态禁用时不编译、不注册。函数表内容与注册顺序与迁移前完全一致。
 */
#include "db/core/func_registry.h"

const FunctionInfo *mm_vector_sql_function_table(size_t *out_count) {
    static FunctionInfo funcs[] = {
        {"VECTOR_SEARCH", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 3, NULL, -1, 0, NULL, NULL, "向量搜索", MODEL_VECTOR, false},
        {"VECTOR_DISTANCE", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 3, NULL, -1, 0, NULL, NULL, "向量距离", MODEL_VECTOR, false},
        {"EMBED_TEXT", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 1, NULL, -1, 0, NULL, NULL, "文本嵌入", MODEL_VECTOR, false},
    };

    *out_count = sizeof(funcs) / sizeof(funcs[0]);
    return funcs;
}
