# REST API 测试
add_executable(test_rest_api rest_api_test.cpp)
target_link_libraries(test_rest_api
    PRIVATE
        vector_api
        db_core
        project_includes
        GTest::gtest_main
)
# 需要 ws2_32 在 Windows 上
if(WIN32)
    target_link_libraries(test_rest_api PRIVATE ws2_32)
endif()

# 向量 API 测试
add_executable(test_vector_api vector_api_test.cpp)
target_link_libraries(test_vector_api
    PRIVATE
        vector_api
        db_core
        project_includes
        GTest::gtest_main
)

# VDB 统一 API 测试
add_executable(test_vdb_api vdb_api_test.cpp)
target_link_libraries(test_vdb_api
    PRIVATE
        vector_api
        db_core
        project_includes
        GTest::gtest_main
        pthread
)

# 工作目录放在构建树：测试数据按 pid 隔离写在 test_data/ 下，
# 不再污染源码树，也不会在多轮执行之间互相看到残留集合。
add_test(NAME test_vdb_api COMMAND test_vdb_api WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME test_rest_api COMMAND test_rest_api WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# ========================================================================
# Vector API 持久化 / 崩溃恢复语义测试
# ========================================================================
# 锁定「快照写成功才允许截断 WAL」这一不变式。
# 工作目录放在构建树，避免像 test_vdb_api 那样把状态留在源码树里互相污染。
add_executable(vector_durability_test vector_durability_test.cpp)
target_link_libraries(vector_durability_test
    PRIVATE
        vector_api
        db_core
        project_includes
        GTest::gtest_main
)

add_test(NAME vector_durability_test COMMAND vector_durability_test
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
