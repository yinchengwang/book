# BRIN 索引测试

include(${ENGINEERING_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_executable(brin_test
    brin_test.cpp
)

target_link_libraries(brin_test
    PRIVATE
        project_includes
        mm_vector_index
        GTest::gtest_main
)

set_target_properties(brin_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)

include(GoogleTest)
gtest_discover_tests(brin_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR} PROPERTIES LABELS "brin_test" DISCOVERY_TIMEOUT 60)