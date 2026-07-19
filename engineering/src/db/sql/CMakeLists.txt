# SQL 模块（解析器 + 计划器 + 执行器）

# 使用普通 add_library 而不是 glob 模式，以便精确控制源文件
# sql_parser.c 有较多兼容性问题，暂时排除
# pgwire.c, mmdb_sdk.c, rest_api.c 使用 Unix 特有头文件，在 Windows 上不编译

add_library(sql_engine STATIC
    planner.c
    sql_executor.c
# 暂时排除 sql_driver.c（有预先存在的 Expr 类型冲突）
#   sql_driver.c
    nodeSeqscan.c
    nodeIndexscan.c
    memctx.c
    expr.c
    expr_interp.c
    executor.c
    cost.c
    parallel.c
    nodeResult.c
    nodeHashjoin.c
    nodeAgg.c
    nodeHashagg.c
    nodeSort.c
    nodeLimit.c
    nodeModifyTable.c
    nodeProjectSet.c
    nodeGather.c
    nodeNestloop.c
    trigger.c
    trigger_functions.c
    partition.c
    tuple_queue.c
    worker_pool.c
    jit.c
)

# 添加头文件搜索路径
target_include_directories(sql_engine PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/parser
)

# 链接依赖库
target_link_libraries(sql_engine PRIVATE
    db_parser_sql_simplified
    db_storage
)

# 设置输出目录
set_target_properties(sql_engine PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)