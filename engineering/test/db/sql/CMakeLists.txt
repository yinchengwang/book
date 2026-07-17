# SQL 模块测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

# SQL 解析器测试（依赖简化解析器）
add_executable(test_sql_parser sql_parser.cpp)
target_include_directories(test_sql_parser PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_sql_parser db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# SQL 计划器测试
add_executable(test_sql_planner test_planner.cpp)
target_include_directories(test_sql_planner PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_sql_planner sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# SQL 执行器测试
add_executable(test_sql_executor test_executor.cpp)
target_include_directories(test_sql_executor PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_sql_executor sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# IndexScan 执行器节点测试
add_executable(test_indexscan test_indexscan.cpp)
target_include_directories(test_indexscan PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_indexscan sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# SeqScan 执行器节点测试
add_executable(test_seqscan test_seqscan.cpp)
target_include_directories(test_seqscan PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_seqscan sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# NestLoop 执行器节点测试
add_executable(test_nestloop test_nestloop.cpp)
target_include_directories(test_nestloop PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_nestloop sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# 并行执行测试
add_executable(test_parallel test_parallel.cpp)
target_include_directories(test_parallel PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_parallel sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# TupleQueue 线程安全队列测试（Task 5.4）
add_executable(test_tuple_queue test_tuple_queue.cpp)
target_include_directories(test_tuple_queue PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_tuple_queue sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# WorkerPool 线程池测试（Task 5.4）
add_executable(test_worker_pool test_worker_pool.cpp)
target_include_directories(test_worker_pool PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_worker_pool sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# 触发器测试
add_executable(test_trigger test_trigger.cpp)
target_include_directories(test_trigger PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_trigger sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# 触发器函数测试（Task 5.5 - 简化版，不依赖 executor.h）
add_executable(test_trigger_functions test_trigger_functions.cpp)
target_include_directories(test_trigger_functions PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_trigger_functions sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# 触发器函数测试（Task 5.5 - 独立测试版）
add_executable(test_trigger_functions_simple test_trigger_functions_simple.cpp)
target_include_directories(test_trigger_functions_simple PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_trigger_functions_simple sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# 分区表路由测试
add_executable(test_partition test_partition.cpp)
target_include_directories(test_partition PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_partition sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# Limit 执行器节点测试
add_executable(test_limit test_limit.cpp)
target_include_directories(test_limit PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_limit sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# Sort 执行器节点测试
add_executable(test_sort test_sort.cpp)
target_include_directories(test_sort PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_sort sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# SQL 集成测试 - 暂时禁用（依赖已禁用的 db_cli_lib）
# add_executable(test_sql_integration sql_integration.cpp)
# target_link_libraries(test_sql_integration db_parser db_storage db_utils db_cli_lib project_includes gtest gtest_main)

# HashAgg 执行器节点测试
add_executable(test_hashagg test_hashagg.cpp)
target_include_directories(test_hashagg PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_hashagg sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# HashJoin 执行器节点测试
add_executable(test_hashjoin test_hashjoin.cpp)
target_include_directories(test_hashjoin PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_hashjoin sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# ModifyTable 执行器节点测试
add_executable(test_modifytable test_modifytable.cpp)
target_include_directories(test_modifytable PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_modifytable sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# 执行引擎集成测试
add_executable(test_executor_integration test_executor_integration.cpp)
target_include_directories(test_executor_integration PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_executor_integration sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# SQL-存储集成测试
add_executable(test_sql_storage_integration test_sql_storage_integration.cpp)
target_include_directories(test_sql_storage_integration PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_sql_storage_integration sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# 存储引擎集成测试（Task 5.1）
add_executable(test_storage_integration test_storage_integration.cpp)
target_include_directories(test_storage_integration PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_storage_integration sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# JIT 性能对比测试（Task 5.6）
add_executable(test_jit_perf test_jit_perf.cpp)
target_include_directories(test_jit_perf PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_jit_perf sql_engine db_parser_sql_simplified db_storage project_includes gtest gtest_main)

# 基准测试（Task 5.3）
add_subdirectory(benchmark)

# SQL 测试
add_custom_target(test_sql
    COMMAND test_sql_parser
    COMMAND test_sql_planner
    COMMAND test_sql_executor
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
    DEPENDS test_sql_parser test_sql_planner test_sql_executor test_indexscan test_seqscan test_nestloop test_hashagg test_hashjoin test_parallel test_tuple_queue test_worker_pool test_trigger test_trigger_functions test_partition test_sort test_limit test_modifytable test_sql_storage_integration test_storage_integration test_jit_perf
)
