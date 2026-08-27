# YANG / NETCONF 测试
cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE YANG_TEST_SOURCES "*.cpp")

add_executable(test_yang ${YANG_TEST_SOURCES})

target_link_libraries(test_yang PRIVATE
    storage_yang
    netconf_server
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)

# 头文件搜索路径
target_include_directories(test_yang PRIVATE
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/yang
    ${ENGINEERING_SOURCE_DIR}/include/db/netconf
)

# Windows MSVC 需要运行时库
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(test_yang PRIVATE /W4 /MDd)
    else()
        target_compile_options(test_yang PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(test_yang PRIVATE -Wall -Wextra -g)
endif()

# 将输出二进制放到构建目录（与项目约定一致）
set_target_properties(test_yang PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)