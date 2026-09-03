# 向量删除模块测试 - 只包含核心功能测试（不依赖 HNSW）

file(GLOB DELETE_TEST_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

# 只包含核心测试文件（排除 HNSW 集成测试）
set(DELETE_CORE_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_delete_bitmap_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_search_filter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gc_vacuum_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rebuild_strategy_test.cpp
)

add_executable(vector_delete_test ${DELETE_CORE_TEST_SOURCES})

target_include_directories(vector_delete_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
)

target_link_libraries(vector_delete_test PRIVATE
    vector_index
    algo-prod
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(vector_delete_test
    PROPERTIES LABELS "vector_index"
)
