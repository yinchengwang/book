# 测试 sparse/BM25/hybrid
# engineering/test/db/storage/CMakeLists.txt

# 编译 test_sparse.c 为可执行文件
add_executable(test_sparse test_sparse.c)

target_link_libraries(test_sparse
    PRIVATE
    storage_sparse
    db_storage
    db_core
)

# 自动发现 test_sparse 中的 main 用例
add_test(NAME test_sparse COMMAND test_sparse)

# ========================================================================
# 空间引擎测试
# ========================================================================

# 空间测试可执行文件
add_executable(test_spatial_engine
    spatial_engine_test.cpp
)

target_link_libraries(test_spatial_engine
    PRIVATE
    storage_spatial    # 空间存储模块
    storage_graph      # 图存储模块（Hilbert 索引依赖）
    db_storage
    db_core
    index_hilbert      # Hilbert 索引
    db_rtree           # R-Tree 索引
    project_includes
    gtest
    gtest_main
)

# 包含必要目录
target_include_directories(test_spatial_engine PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/storage
    ${ENGINEERING_SOURCE_DIR}/include/db/index
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

# Windows MSVC 运行时库
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(test_spatial_engine PRIVATE /W4 /MDd)
    else()
        target_compile_options(test_spatial_engine PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(test_spatial_engine PRIVATE -Wall -Wextra)
endif()

# 输出到构建目录
set_target_properties(test_spatial_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 注册测试
add_test(NAME test_spatial_engine COMMAND test_spatial_engine)

# ========================================================================
# C0-1 统一并发原语推广并发回归测试
# ========================================================================

add_executable(c0_1_concurrency_test
    c0_1_concurrency_test.cpp
)

target_link_libraries(c0_1_concurrency_test
    PRIVATE
    storage_vector
    storage_ts
    storage_doc
    storage_graph
    db_storage
    db_mvcc
    project_includes
    Threads::Threads
    gtest
    gtest_main
)

target_include_directories(c0_1_concurrency_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/storage
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(c0_1_concurrency_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(c0_1_concurrency_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(c0_1_concurrency_test PRIVATE -Wall -Wextra)
endif()

set_target_properties(c0_1_concurrency_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME c0_1_concurrency_test COMMAND c0_1_concurrency_test)

# ========================================================================
# C0-2 统一 WAL 恢复入口 API 测试
# ========================================================================

add_executable(c0_2_wal_recover_test
    c0_2_wal_recover_test.cpp
)

target_link_libraries(c0_2_wal_recover_test
    PRIVATE
    storage_wal
    db_storage
    project_includes
    gtest
    gtest_main
)

target_include_directories(c0_2_wal_recover_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/storage
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(c0_2_wal_recover_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(c0_2_wal_recover_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(c0_2_wal_recover_test PRIVATE -Wall -Wextra)
endif()

set_target_properties(c0_2_wal_recover_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME c0_2_wal_recover_test COMMAND c0_2_wal_recover_test)

# ========================================================================
# C1-1 关系模态 TID 管道修复测试
# ========================================================================

add_executable(c1_1_tid_pipeline_test
    c1_1_tid_pipeline_test.cpp
)

target_link_libraries(c1_1_tid_pipeline_test
    PRIVATE
    storage_heap    # heap AM
    db_storage
    project_includes
    gtest
    gtest_main
)

target_include_directories(c1_1_tid_pipeline_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/storage
    ${ENGINEERING_SOURCE_DIR}/include/db/storage/access
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(c1_1_tid_pipeline_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(c1_1_tid_pipeline_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(c1_1_tid_pipeline_test PRIVATE -Wall -Wextra)
endif()

set_target_properties(c1_1_tid_pipeline_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME c1_1_tid_pipeline_test COMMAND c1_1_tid_pipeline_test)

# ========================================================================
# C1-2 faiss_hnsw 并发与 IP 度量测试
# ========================================================================

add_executable(c1_2_hnsw_test
    c1_2_hnsw_test.cpp
)

target_link_libraries(c1_2_hnsw_test
    PRIVATE
    vector_index_faiss_hnsw
    db_storage
    project_includes
    Threads::Threads
    gtest
    gtest_main
)

target_include_directories(c1_2_hnsw_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/index
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(c1_2_hnsw_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(c1_2_hnsw_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(c1_2_hnsw_test PRIVATE -Wall -Wextra)
endif()

set_target_properties(c1_2_hnsw_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME c1_2_hnsw_test COMMAND c1_2_hnsw_test)

# ========================================================================
# C1-3 KV 并发正确性测试
# ========================================================================

add_executable(c1_3_kv_concurrency_test
    c1_3_kv_concurrency_test.cpp
)

target_link_libraries(c1_3_kv_concurrency_test
    PRIVATE
    db_storage
    project_includes
    Threads::Threads
    gtest
    gtest_main
)

target_include_directories(c1_3_kv_concurrency_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(c1_3_kv_concurrency_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(c1_3_kv_concurrency_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(c1_3_kv_concurrency_test PRIVATE -Wall -Wextra)
endif()

set_target_properties(c1_3_kv_concurrency_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME c1_3_kv_concurrency_test COMMAND c1_3_kv_concurrency_test)

# ========================================================================
# C2-3 PageRank 悬挂节点测试
# ========================================================================

add_executable(c2_3_pagerank_test
    c2_3_pagerank_test.cpp
)

target_link_libraries(c2_3_pagerank_test
    PRIVATE
    graph_algo
    graph_storage
    db_storage
    project_includes
    gtest
    gtest_main
)

target_include_directories(c2_3_pagerank_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(c2_3_pagerank_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(c2_3_pagerank_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(c2_3_pagerank_test PRIVATE -Wall -Wextra)
endif()

set_target_properties(c2_3_pagerank_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME c2_3_pagerank_test COMMAND c2_3_pagerank_test)

# ========================================================================
# 文档聚合管道测试
# ========================================================================

add_subdirectory(doc)
