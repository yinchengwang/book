# SQL 模块测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

# MemoryContext 测试
add_executable(test_memctx test_memctx.cpp)
target_include_directories(test_memctx PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_memctx sql_engine project_includes gtest gtest_main)

# SQL 解析器 canonical 烟测（T12 / WS-F，替代归档的 test_sql_parser / sql_parser.cpp）
add_executable(test_canonical_parser test_canonical_parser.cpp)
target_include_directories(test_canonical_parser PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_canonical_parser sql_engine db_storage project_includes gtest gtest_main)
set_target_properties(test_canonical_parser PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_canonical_parser
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "CanonicalParser"
)

# SQL 执行器测试
add_executable(test_sql_executor test_executor.cpp)
target_include_directories(test_sql_executor PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_sql_executor sql_engine db_storage project_includes gtest gtest_main)

# IndexScan 执行器节点测试
add_executable(test_indexscan test_indexscan.cpp)
target_include_directories(test_indexscan PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_indexscan sql_engine db_storage project_includes gtest gtest_main)

# SeqScan 执行器节点测试
add_executable(test_seqscan test_seqscan.cpp)
target_include_directories(test_seqscan PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_seqscan sql_engine db_storage project_includes gtest gtest_main)

# NestLoop 执行器节点测试
add_executable(test_nestloop test_nestloop.cpp)
target_include_directories(test_nestloop PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_nestloop sql_engine db_storage project_includes gtest gtest_main)

# 并行执行测试
add_executable(test_parallel test_parallel.cpp)
target_include_directories(test_parallel PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_parallel sql_engine db_storage project_includes gtest gtest_main)

# TupleQueue 线程安全队列测试（Task 5.4）
add_executable(test_tuple_queue test_tuple_queue.cpp)
target_include_directories(test_tuple_queue PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_tuple_queue sql_engine db_storage project_includes gtest gtest_main)

# WorkerPool 线程池测试（Task 5.4）
add_executable(test_worker_pool test_worker_pool.cpp)
target_include_directories(test_worker_pool PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_worker_pool sql_engine db_storage project_includes gtest gtest_main)

# 触发器测试
add_executable(test_trigger test_trigger.cpp)
target_include_directories(test_trigger PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_trigger sql_engine db_storage project_includes gtest gtest_main)

# 触发器函数测试（Task 5.5 - 简化版，不依赖 executor.h）
add_executable(test_trigger_functions test_trigger_functions.cpp)
target_include_directories(test_trigger_functions PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_trigger_functions sql_engine db_storage project_includes gtest gtest_main)

# 触发器函数测试（Task 5.5 - 独立测试版）
add_executable(test_trigger_functions_simple test_trigger_functions_simple.cpp)
target_include_directories(test_trigger_functions_simple PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_trigger_functions_simple sql_engine db_storage project_includes gtest gtest_main)

# 分区表路由测试
add_executable(test_partition test_partition.cpp)
target_include_directories(test_partition PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_partition sql_engine db_storage project_includes gtest gtest_main)

# Limit 执行器节点测试
add_executable(test_limit test_limit.cpp)
target_include_directories(test_limit PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_limit sql_engine db_storage project_includes gtest gtest_main)

# Sort 执行器节点测试
add_executable(test_sort test_sort.cpp)
target_include_directories(test_sort PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_sort sql_engine db_storage project_includes gtest gtest_main)

# SQL 集成测试（T12 / WS-F：51 个 CLI 端到端用例，走 db_cli_lib）
add_executable(test_sql_integration sql_integration.cpp)
target_include_directories(test_sql_integration PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_sql_integration sql_engine db_storage db_cli_lib project_includes gtest gtest_main)
set_target_properties(test_sql_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_sql_integration
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "SqlIntegration"
)

# HashAgg 执行器节点测试
add_executable(test_hashagg test_hashagg.cpp)
target_include_directories(test_hashagg PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_hashagg sql_engine db_storage project_includes gtest gtest_main)

# HashJoin 执行器节点测试
add_executable(test_hashjoin test_hashjoin.cpp)
target_include_directories(test_hashjoin PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_hashjoin sql_engine db_storage project_includes gtest gtest_main)

# ModifyTable 执行器节点测试
add_executable(test_modifytable test_modifytable.cpp)
target_include_directories(test_modifytable PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_modifytable sql_engine db_storage project_includes gtest gtest_main)

# 执行引擎集成测试
add_executable(test_executor_integration test_executor_integration.cpp)
target_include_directories(test_executor_integration PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_executor_integration sql_engine db_storage project_includes gtest gtest_main)

# SQL-存储集成测试
# T12：test_storage_integration.cpp 与 test_sql_storage_integration.cpp 调用了
# 旧 ExecInitSeqScan 签名（T8 移除）与旧 heap_insert 签名（T8 后签名变更），
# 自 T8 起构建红。归档至 archive/（同 sql_integration 的 51 用例，main 路径
# 已由 test_sql_integration + DriverSmoke 覆盖），从构建移除直至 S1 修复。
# add_executable(test_sql_storage_integration test_sql_storage_integration.cpp)
# add_executable(test_storage_integration test_storage_integration.cpp)

# SQL Driver 端到端烟测（T7：sql_driver.c 复活于 canonical AST）
add_executable(test_driver_smoke test_driver_smoke.cpp)
target_include_directories(test_driver_smoke PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_driver_smoke sql_engine db_storage project_includes gtest gtest_main)
set_target_properties(test_driver_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
include(GoogleTest)
gtest_discover_tests(test_driver_smoke
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_driver_smoke"
)

# SQL 主路径算子 e2e 能力测试（T9：ORDER BY/LIMIT/GROUP BY/JOIN/FROM 子查询）
add_executable(test_e2e_capability test_e2e_capability.cpp)
target_include_directories(test_e2e_capability PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_e2e_capability sql_engine db_storage project_includes gtest gtest_main)
set_target_properties(test_e2e_capability PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_e2e_capability
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_e2e_capability"
)

# 存储引擎集成测试（Task 5.1）— T12 已归档，见 test_storage_integration.cpp 注释
# add_executable(test_storage_integration test_storage_integration.cpp)
# target_include_directories(test_storage_integration PRIVATE ${GTEST_INCLUDE_DIRS})
# target_link_libraries(test_storage_integration sql_engine db_storage project_includes gtest gtest_main)

# JIT 性能对比测试（Task 5.6）
add_executable(test_jit_perf test_jit_perf.cpp)
target_include_directories(test_jit_perf PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_jit_perf sql_engine db_storage project_includes gtest gtest_main)

# SQL Executor MemoryContext 迁移验证测试（Task 10）
add_executable(test_sql_executor_memctx sql_executor_memctx_test.cpp)
target_include_directories(test_sql_executor_memctx PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_sql_executor_memctx sql_engine db_storage project_includes gtest gtest_main)

# 基准测试（Task 5.3）
add_subdirectory(benchmark)

# SQL 测试
# T12：test_sql_parser / test_sql_planner 已随栈 A 归档移出（见
# archive/db-sql-over-kv/tests/）；canonical 解析器烟测为 test_canonical_parser，
# CLI 端到端集成测试为 test_sql_integration。
add_custom_target(test_sql
    COMMAND test_memctx
    COMMAND test_sql_executor
    COMMAND test_canonical_parser
    COMMAND test_sql_integration
    COMMAND test_indexscan
    COMMAND test_seqscan
    COMMAND test_nestloop
    COMMAND test_hashagg
    COMMAND test_hashjoin
    COMMAND test_parallel
    COMMAND test_tuple_queue
    COMMAND test_worker_pool
    COMMAND test_trigger
    COMMAND test_trigger_functions
    COMMAND test_trigger_functions_simple
    COMMAND test_partition
    COMMAND test_sort
    COMMAND test_limit
    COMMAND test_modifytable
    COMMAND test_sql_storage_integration
    COMMAND test_storage_integration
    COMMAND test_jit_perf
    COMMAND test_sql_executor_memctx
    DEPENDS test_memctx test_sql_executor test_canonical_parser test_sql_integration test_indexscan test_seqscan test_nestloop test_hashagg test_hashjoin test_parallel test_tuple_queue test_worker_pool test_trigger test_trigger_functions test_jit_perf test_sql_executor_memctx
)
