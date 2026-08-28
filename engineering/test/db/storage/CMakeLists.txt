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
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # 使用 vendored googletest
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_part/googletest ${CMAKE_BINARY_DIR}/third_party/googletest EXCLUDE_FROM_ALL)
    set(GTEST_LIBRARIES gtest gtest_main)
    set(GTEST_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include)
endif()

target_link_libraries(blob_engine_test PRIVATE storage_blob db_core project_includes)

if(GTest_FOUND)
    target_link_libraries(blob_engine_test PRIVATE GTest::GTest GTest::Main)
else()
    target_link_libraries(blob_engine_test PRIVATE ${GTEST_LIBRARIES})
endif()

target_include_directories(blob_engine_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)

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
