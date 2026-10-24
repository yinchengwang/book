# Queue 测试

include(${ENGINEERING_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_test(queue_gtest QUEUE_TEST_SOURCES
    algo-prod
)