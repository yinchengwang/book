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

add_test(NAME test_vdb_api COMMAND test_vdb_api WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_rest_api COMMAND test_rest_api WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
