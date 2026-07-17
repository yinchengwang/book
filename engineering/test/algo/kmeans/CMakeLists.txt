# KMeans测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_test(kmeans_gtest KMEANS_TEST_SOURCES
    algo-prod
)
