# SDK 测试构建配置
cmake_minimum_required(VERSION 3.20)

# 注意：googletest 已被 engineering/CMakeLists.txt 添加，gtest target 在父作用域可见
# 注意：src/sdk 已被 src/CMakeLists.txt 添加，mmsdk target 在父作用域可见

# 测试用例列表
set(SDK_TESTS
    mmdb_core_test
    mmdb_vectors_test
    mmdb_graph_test
    mmdb_timeseries_test
    mmdb_text_test
    mmdb_filter_test
    mmdb_raii_test
    rrf_test   # P3-T1.1：RRF 融合测试，源码位于 hybrid/ 子目录
)

# 为每个测试源文件创建可执行文件
foreach(test_name IN LISTS SDK_TESTS)
    # rrf_test 源码位于 hybrid/ 子目录（P3-T1.1 混合检索分类）
    if(test_name STREQUAL "rrf_test")
        add_executable(${test_name} hybrid/${test_name}.cpp)
    else()
        add_executable(${test_name} ${test_name}.cpp)
    endif()
    target_link_libraries(${test_name} PRIVATE mmsdk gtest_main)
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
    # 输出到 build 目录（不污染源码）
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_tests
    )
endforeach()