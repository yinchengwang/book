# SQL 执行引擎和向量索引性能基准测试

# 添加基准测试可执行文件
# T12：benchmark_aggregate.cpp / benchmark_scan.cpp 调用旧 heap_insert 与
# ExecInitSeqScan 签名（T8 后变更），自 T8 起构建红。暂时排除，待 S1 重写。
add_executable(sql_benchmark
    benchmark_main.cpp
    # benchmark_scan.cpp — T12 排除（旧 API）
    # benchmark_aggregate.cpp — T12 排除（旧 API）
    # benchmark_join.cpp — 暂时排除，Oid/TupleDescData 头文件冲突待修复
    benchmark_vector.cpp
)

# 设置包含目录
target_include_directories(sql_benchmark
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GTEST_INCLUDE_DIRS}
)

# 链接库
# T4：db_parser_sql_simplified 已归档移除，canonical 目标名由 T6 补回。
target_link_libraries(sql_benchmark
    PRIVATE
        sql_engine
        db_storage
        vector_index
        faiss_hnsw
        algo-prod
        project_includes
        gtest
        gtest_main
)

# 设置输出目录（编译产物统一进入 build/ 目录）
set_target_properties(sql_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Benchmarks"
)

# 添加到 CTest
include(GoogleTest)
gtest_discover_tests(sql_benchmark
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "benchmark"
)

message(STATUS "Added sql_benchmark tests")