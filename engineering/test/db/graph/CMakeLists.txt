cmake_minimum_required(VERSION 3.20)

# 图数据库测试
file(GLOB_RECURSE GRAPH_TEST_SOURCES "*.cpp")

add_executable(test_graph_basic_test ${GRAPH_TEST_SOURCES})

target_link_libraries(test_graph_basic_test PRIVATE
    db_graph
    db_storage
    project_includes
    gtest
    gtest_main
)

# 复制 gtest 配置
target_include_directories(test_graph_basic_test PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/googletest/googletest/include
    ${CMAKE_SOURCE_DIR}/third_party/googletest/googlemock/include
)

# Windows MSVC 需要运行时库
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        target_compile_options(test_graph_basic_test PRIVATE /W4 /MDd)
    else()
        target_compile_options(test_graph_basic_test PRIVATE /W4 /MD)
    endif()
else()
    target_compile_options(test_graph_basic_test PRIVATE -Wall -Wextra -g)
endif()

# 将输出二进制放到源码目录（与项目约定一致）
set_target_properties(test_graph_basic_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${ENGINEERING_SOURCE_DIR}/test/db/graph
)
