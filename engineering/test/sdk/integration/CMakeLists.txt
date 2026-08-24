# SDK 集成测试构建配置
cmake_minimum_required(VERSION 3.20)

# 集成测试：跨语言一致性 + 性能基准 + 并发安全
add_executable(cross_lang_consistency_test cross_lang_consistency_test.cpp)
target_link_libraries(cross_lang_consistency_test PRIVATE mmsdk gtest_main)
target_include_directories(cross_lang_consistency_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(cross_lang_consistency_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME cross_lang_consistency_test COMMAND cross_lang_consistency_test)

# 向量索引选择器集成测试
add_executable(selector_integration_test selector_integration_test.cpp)
target_link_libraries(selector_integration_test PRIVATE mmsdk gtest_main)
target_include_directories(selector_integration_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(selector_integration_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME selector_integration_test COMMAND selector_integration_test)

# 向量 KNN 阶梯基准测试（T5.8）
add_executable(staircase_benchmark staircase_benchmark.cpp)
target_link_libraries(staircase_benchmark PRIVATE mmsdk gtest_main)
target_include_directories(staircase_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(staircase_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME staircase_benchmark COMMAND staircase_benchmark)

# P5-2：CRoaring bitmap 兼容层单元测试
add_executable(roaring_bitmap_test roaring_bitmap_test.cpp)
target_link_libraries(roaring_bitmap_test PRIVATE gtest_main)
target_include_directories(roaring_bitmap_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_part/croaring
)
set_target_properties(roaring_bitmap_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME roaring_bitmap_test COMMAND roaring_bitmap_test)