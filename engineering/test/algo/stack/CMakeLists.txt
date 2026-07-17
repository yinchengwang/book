# Stack 测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_test(stack_gtest STACK_TEST_SOURCES
    algo-prod
)