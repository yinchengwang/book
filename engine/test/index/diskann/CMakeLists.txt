# DiskANN 测试子目录
#
# 测试文件列表（按功能模块分组）：
# - test_config.cpp: 统一配置测试
# - test_partition.cpp: 分区策略测试
# - test_merge.cpp: Merge-Vamana 测试
# - test_spann.cpp: SPANN 测试
# - test_fresh.cpp: FreshDiskANN 测试

# 所有测试文件
set(DISKANN_TEST_FILES
    test_config.cpp
    test_partition.cpp
    test_merge.cpp
    test_spann.cpp
    test_fresh.cpp
)

# 创建测试可执行文件
add_executable(test_diskann_extended ${DISKANN_TEST_FILES})

# 链接库
target_link_libraries(test_diskann_extended
    PRIVATE
    project_includes
    diskann_index
    algo
    GTest::gtest
    GTest::gtest_main
)

# 添加测试
add_test(NAME diskann_extended_test COMMAND test_diskann_extended)