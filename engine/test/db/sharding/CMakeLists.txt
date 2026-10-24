cmake_minimum_required(VERSION 3.20)

# 分片测试
file(GLOB_RECURSE SHARDING_TEST_SOURCES "*.cpp")

add_executable(test_sharding ${SHARDING_TEST_SOURCES})

target_link_libraries(test_sharding PRIVATE
    db_sharding
    # db_sharding 依赖 KV 存储引擎的 kv_open / kv_put / kv_get 等符号，
    # 这些符号由 db_storage 收集的 storage_kv OBJECT 库提供。
    db_storage
    project_includes
    gtest
    gtest_main
)

# Windows MSVC 需要运行时库
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(test_sharding PRIVATE /W4 /MDd)
    else()
        target_compile_options(test_sharding PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(test_sharding PRIVATE -Wall -Wextra -g)
endif()

# 将输出二进制放到构建目录
set_target_properties(test_sharding PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
