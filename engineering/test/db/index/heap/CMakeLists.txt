# heap_vector_store 单元测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_executable(heap_vector_store_test
    heap_vector_store_test.cpp
)

target_link_libraries(heap_vector_store_test
    PRIVATE
        project_includes
        heap_vector_store
        storage_backend_memory
        GTest::gtest_main
)

set_target_properties(heap_vector_store_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)

include(GoogleTest)
gtest_discover_tests(heap_vector_store_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "heap_vector_store_test"
)