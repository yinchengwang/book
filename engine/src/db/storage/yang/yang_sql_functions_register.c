/**
 * @file yang_sql_functions_register.c
 * @brief 树模态内置 SQL 函数注册（Task 3 自 func_registry.c 迁入）
 *
 * 由构建期生成的 mm_builtin_engines.c 调用（mm_register_builtin_sql_functions），
 * 模态禁用时不编译、不注册。函数表内容与注册顺序与迁移前完全一致。
 */
#include "db/core/func_registry.h"

int mm_tree_register_sql_functions(void) {
    static FunctionInfo funcs[] = {
        {"TREE_ANCESTORS", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 1, NULL, -1, 0, NULL, NULL, "祖先节点", MODEL_TREE, false},
        {"TREE_DESCENDANTS", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 2, NULL, -1, 0, NULL, NULL, "后代节点", MODEL_TREE, false},
        {"TREE_DEPTH", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 1, NULL, -1, 0, NULL, NULL, "节点深度", MODEL_TREE, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}
