# Queue 测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_test(queue_gtest QUEUE_TEST_SOURCES
    algo-prod
)