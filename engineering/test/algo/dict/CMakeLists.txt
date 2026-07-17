# Dict 测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_test(dict_gtest DICT_TEST_SOURCES
    algo-prod
)