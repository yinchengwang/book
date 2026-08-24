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