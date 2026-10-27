/**
 * @file graph_sql_functions_register.c
 * @brief 图模态内置 SQL 函数注册（Task 3 自 func_registry.c 迁入）
 *
 * 由构建期生成的 mm_builtin_engines.c 调用（mm_register_builtin_sql_functions），
 * 模态禁用时不编译、不注册。函数表内容与注册顺序与迁移前完全一致。
 */
#include "db/core/func_registry.h"

int mm_graph_register_sql_functions(void) {
    static FunctionInfo funcs[] = {
        {"GRAPH_TRAVERSE", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 2, NULL, -1, 0, NULL, NULL, "图遍历", MODEL_GRAPH, false},
        {"GRAPH_SHORTEST_PATH", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 3, NULL, -1, 0, NULL, NULL, "最短路径", MODEL_GRAPH, false},
        {"GRAPH_PAGERANK", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 1, NULL, -1, 0, NULL, NULL, "PageRank", MODEL_GRAPH, false},
        {"GRAPH_NEIGHBORS", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 2, NULL, -1, 0, NULL, NULL, "邻居查询", MODEL_GRAPH, false},
    };

    for (size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++) {
        func_registry_register(&funcs[i]);
    }
    return 0;
}
