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
# 文档聚合管道测试
# ========================================================================

add_subdirectory(doc)

# ========================================================================
# Blob 存储引擎测试
# ========================================================================

add_executable(blob_engine_test
    blob_engine_test.cpp
)

# 使用 CMake 查找 GTest（conda 或 vendored）
find_package(GTest REQUIRED)
target_link_libraries(blob_engine_test PRIVATE GTest::GTest GTest::Main)

# 链接 Blob 存储引擎库
target_link_libraries(blob_engine_test PRIVATE storage_blob db_core project_includes)

target_include_directories(blob_engine_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)

# 添加 GTest include 路径
if(GTest_FOUND)
    target_include_directories(blob_engine_test PRIVATE ${GTEST_INCLUDE_DIRS})
else()
    target_include_directories(blob_engine_test PRIVATE
        ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
        ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
    )
endif()

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(blob_engine_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(blob_engine_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(blob_engine_test PRIVATE -Wall -Wextra)
endif()

set_target_properties(blob_engine_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME blob_engine_test COMMAND blob_engine_test)

# WAL 集成测试
add_executable(test_storage_wal wal_integration_test.c)
target_link_libraries(test_storage_wal
    PRIVATE
    db_storage
    db_core
    project_includes
)
target_include_directories(test_storage_wal PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)
add_test(NAME test_storage_wal COMMAND test_storage_wal)

# ========================================================================
# KV 测试（C3-5 T22）
# ========================================================================
add_executable(test_storage_kv kv.cpp)
target_link_libraries(test_storage_kv
    PRIVATE
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)
target_include_directories(test_storage_kv PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)
add_test(NAME test_storage_kv COMMAND test_storage_kv)

# ========================================================================
# KV 并发和高级功能测试（C3-5 T22）
# ========================================================================
add_executable(test_kv_concurrency kv_concurrency_test.cpp)
target_link_libraries(test_kv_concurrency
    PRIVATE
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)
target_include_directories(test_kv_concurrency PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)
add_test(NAME test_kv_concurrency COMMAND test_kv_concurrency)

# ========================================================================
# TID 管道测试
# ========================================================================
add_executable(tid_pipe_test tid_pipe_test.c)
target_link_libraries(tid_pipe_test
    PRIVATE
    db_storage
    db_core
    project_includes
)
target_include_directories(tid_pipe_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)
add_test(NAME tid_pipe_test COMMAND tid_pipe_test)

# ========================================================================
# RDF SPARQL 解析器测试（T39: FILTER, OPTIONAL, GROUP BY）
# ========================================================================
add_executable(test_rdf_sparql test_rdf_sparql.c)
target_link_libraries(test_rdf_sparql
    PRIVATE
    storage_rdf
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)
target_include_directories(test_rdf_sparql PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)
set_target_properties(test_rdf_sparql PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_test(NAME test_rdf_sparql COMMAND test_rdf_sparql)

# ========================================================================
# RDF 引擎综合测试
# ========================================================================
add_executable(test_rdf_engine rdf_engine_test.cpp)
target_link_libraries(test_rdf_engine
    PRIVATE
    storage_rdf
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)
target_include_directories(test_rdf_engine PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/src/db/storage/rdf
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)
set_target_properties(test_rdf_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_test(NAME test_rdf_engine COMMAND test_rdf_engine)

# ========================================================================
# Stream 引擎测试
# ========================================================================
add_executable(stream_engine_test
    stream_engine_test.cpp
)

target_link_libraries(stream_engine_test
    PRIVATE
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)

target_include_directories(stream_engine_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(stream_engine_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(stream_engine_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(stream_engine_test PRIVATE -Wall -Wextra)
endif()

set_target_properties(stream_engine_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_test(NAME stream_engine_test COMMAND stream_engine_test)

# ========================================================================
# Columnar 引擎测试
# ========================================================================
add_executable(columnar_engine_test
    columnar_engine_test.cpp
)

target_link_libraries(columnar_engine_test
    PRIVATE
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)

target_include_directories(columnar_engine_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(columnar_engine_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(columnar_engine_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(columnar_engine_test PRIVATE -Wall -Wextra)
endif()

set_target_properties(columnar_engine_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_test(NAME columnar_engine_test COMMAND columnar_engine_test)

# ========================================================================
# Graph Storage Engine Tests (C2-3 T9)
# ========================================================================
add_executable(test_graph
    graph_test.cpp
)

target_link_libraries(test_graph
    PRIVATE
    storage_graph
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)

target_include_directories(test_graph PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/storage
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(test_graph PRIVATE /W4 /MDd)
    else()
        target_compile_options(test_graph PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(test_graph PRIVATE -Wall -Wextra)
endif()

set_target_properties(test_graph PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_test(NAME test_graph COMMAND test_graph)

# ========================================================================
# Vector Index Tests (HNSW-PQ, IVF-PQ, Sparse, BM25, Hybrid)
# ========================================================================
add_executable(test_vector_index
    vector_index_test.cpp
)

target_link_libraries(test_vector_index
    PRIVATE
    index_hnsw_pq
    index_ivf_pq
    storage_sparse
    db_core
    db_storage
    project_includes
    gtest
    gtest_main
)

target_include_directories(test_vector_index PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/index
    ${ENGINEERING_SOURCE_DIR}/include/db/storage
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(test_vector_index PRIVATE /W4 /MDd)
    else()
        target_compile_options(test_vector_index PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(test_vector_index PRIVATE -Wall -Wextra)
endif()

set_target_properties(test_vector_index PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_test(NAME test_vector_index COMMAND test_vector_index)
