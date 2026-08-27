cmake_minimum_required(VERSION 3.20)

# 列族存储引擎测试
file(GLOB_RECURSE CF_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(test_cf_engine ${CF_TEST_SOURCES})

target_link_libraries(test_cf_engine PRIVATE
    cf_engine
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)

# 复制 gtest 配置
target_include_directories(test_cf_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googlemock/include
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/cf
)

# Windows MSVC 需要运行时库
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(test_cf_engine PRIVATE /W4 /MDd)
    else()
        target_compile_options(test_cf_engine PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(test_cf_engine PRIVATE -Wall -Wextra -g)
endif()

# 将输出二进制放到构建目录（与项目约定一致）
set_target_properties(test_cf_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)