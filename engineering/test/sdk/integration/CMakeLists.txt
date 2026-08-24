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

# P6-M1.1：分页 API 测试
add_executable(pagination_test pagination_test.cpp)
target_link_libraries(pagination_test PRIVATE mmsdk gtest_main)
target_include_directories(pagination_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(pagination_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME pagination_test COMMAND pagination_test)

# P6-M1.2：监控指标测试
add_executable(metrics_test metrics_test.cpp)
target_link_libraries(metrics_test PRIVATE mmsdk gtest_main)
target_include_directories(metrics_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(metrics_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME metrics_test COMMAND metrics_test)

# P6-M4.1：通用聚合框架测试
add_executable(aggregate_test aggregate_test.cpp)
target_link_libraries(aggregate_test PRIVATE mmsdk gtest_main)
target_include_directories(aggregate_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(aggregate_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME aggregate_test COMMAND aggregate_test)

# P6-M2.2：ACID 事务测试
add_executable(txn_test txn_test.cpp)
target_link_libraries(txn_test PRIVATE mmsdk gtest_main)
target_include_directories(txn_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(txn_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME txn_test COMMAND txn_test)

# P6-M2.1：数据库备份/恢复测试
add_executable(backup_test backup_test.cpp)
target_link_libraries(backup_test PRIVATE mmsdk gtest_main)
target_include_directories(backup_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(backup_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME backup_test COMMAND backup_test)

# P6-M3.1：Raft 复制协议测试
add_executable(replication_test replication_test.cpp)
target_link_libraries(replication_test PRIVATE mmsdk gtest_main)
target_include_directories(replication_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(replication_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME replication_test COMMAND replication_test)

# P6-M3.2：一致性哈希分片测试
add_executable(sharding_test sharding_test.cpp)
target_link_libraries(sharding_test PRIVATE mmsdk gtest_main)
target_include_directories(sharding_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(sharding_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME sharding_test COMMAND sharding_test)

# P6-M3.3：命名空间隔离与资源配额测试
add_executable(namespace_test namespace_test.cpp)
target_link_libraries(namespace_test PRIVATE mmsdk gtest_main)
target_include_directories(namespace_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
set_target_properties(namespace_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk_integration_tests
)
add_test(NAME namespace_test COMMAND namespace_test)