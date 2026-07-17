# SQL 执行引擎和向量索引性能基准测试

# 添加基准测试可执行文件
add_executable(sql_benchmark
    benchmark_main.cpp
    benchmark_scan.cpp
    benchmark_join.cpp
    benchmark_aggregate.cpp
    benchmark_vector.cpp
)

# 设置包含目录
target_include_directories(sql_benchmark
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GTEST_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(sql_benchmark
    PRIVATE
        sql_engine
        db_parser_sql_simplified
        db_storage
        vector_index
        hnsw
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