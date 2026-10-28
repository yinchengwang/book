# GIN 索引测试

include(${ENGINEERING_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_executable(gin_test
    gin_test.cpp
)

target_link_libraries(gin_test
    PRIVATE
        project_includes
        mm_vector_index
        GTest::gtest_main
)

set_target_properties(gin_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)

include(GoogleTest)
gtest_discover_tests(gin_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "gin_test"
)
