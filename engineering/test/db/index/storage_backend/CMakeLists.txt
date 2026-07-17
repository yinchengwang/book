# 存储后端抽象层单元测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_executable(storage_backend_test
    storage_backend_test.cpp
)

target_link_libraries(storage_backend_test
    PRIVATE
        project_includes
        storage_backend_common
        storage_backend_memory
        storage_backend_pagefile
        storage_backend_mmap
        GTest::gtest_main
)

set_target_properties(storage_backend_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)

include(GoogleTest)
gtest_discover_tests(storage_backend_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "storage_backend_test"
)