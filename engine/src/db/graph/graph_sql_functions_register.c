/**
 * @file graph_sql_functions_register.c
 * @brief 图模态内置 SQL 函数注册（Task 3 自 func_registry.c 迁入）
 *
 * mm_builtin_engines.c 经 func_registry_register_table 下行注册（模态零 L1 调用），
 * 模态禁用时不编译、不注册。函数表内容与注册顺序与迁移前完全一致。
 */
#include "db/core/func_registry.h"

const FunctionInfo *mm_graph_sql_function_table(size_t *out_count) {
    static FunctionInfo funcs[] = {
        {"GRAPH_TRAVERSE", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 2, NULL, -1, 0, NULL, NULL, "图遍历", MODEL_GRAPH, false},
        {"GRAPH_SHORTEST_PATH", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 3, NULL, -1, 0, NULL, NULL, "最短路径", MODEL_GRAPH, false},
        {"GRAPH_PAGERANK", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 1, NULL, -1, 0, NULL, NULL, "PageRank", MODEL_GRAPH, false},
        {"GRAPH_NEIGHBORS", "public", FUNC_CLASS_NORMAL, FUNC_RESULT_TABLE, 2, NULL, -1, 0, NULL, NULL, "邻居查询", MODEL_GRAPH, false},
    };

    *out_count = sizeof(funcs) / sizeof(funcs[0]);
    return funcs;
}
