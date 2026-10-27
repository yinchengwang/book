/**
 * @file spatial_sql_functions_register.c
 * @brief 空间模态内置 SQL 函数注册（Task 3 自 func_registry.c 迁入）
 *
 * 由构建期生成的 mm_builtin_engines.c 调用（mm_register_builtin_sql_functions），
 * 模态禁用时不编译、不注册。函数表内容与注册顺序与迁移前完全一致。
 */
#include "db/core/func_registry.h"

int mm_spatial_register_sql_functions(void) {
    static FunctionInfo funcs[] = {
        {"ST_Distance", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "空间距离", MODEL_SPATIAL, false},
        {"ST_Contains", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "空间包含", MODEL_SPATIAL, false},
        {"ST_Within", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "空间被包含", MODEL_SPATIAL, false},
        {"ST_Intersects", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "空间相交", MODEL_SPATIAL, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}
