/**
 * @file ts_sql_functions_register.c
 * @brief 时序模态内置 SQL 函数注册（Task 3 自 func_registry.c 迁入）
 *
 * 由构建期生成的 mm_builtin_engines.c 调用（mm_register_builtin_sql_functions），
 * 模态禁用时不编译、不注册。函数表内容与注册顺序与迁移前完全一致。
 */
#include "db/core/func_registry.h"

int mm_ts_register_sql_functions(void) {
    static FunctionInfo funcs[] = {
        {"TIME_SERIES_AGG", "public", FUNC_CLASS_AGGREGATE, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "时序聚合", MODEL_TIMESERIES, false},
        {"FIRST", "public", FUNC_CLASS_AGGREGATE, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "首值", MODEL_TIMESERIES, false},
        {"LAST", "public", FUNC_CLASS_AGGREGATE, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "末值", MODEL_TIMESERIES, false},
        {"RATE", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_SCALAR, 2, NULL, -1, 0, NULL, NULL, "变化率", MODEL_TIMESERIES, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}
