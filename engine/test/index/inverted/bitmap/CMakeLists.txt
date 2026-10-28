# Bitmap 索引测试

include(${ENGINEERING_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_executable(bitmap_test
    bitmap_test.cpp
)

target_link_libraries(bitmap_test
    PRIVATE
        project_includes
        mm_vector_index
        GTest::gtest_main
)

set_target_properties(bitmap_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)

include(GoogleTest)
gtest_discover_tests(bitmap_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "bitmap_test"
)
