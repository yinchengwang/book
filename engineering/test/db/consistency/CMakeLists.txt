# Consistency 模块测试

add_executable(consistency_test consistency_test.cpp)

target_include_directories(consistency_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/consensus
    ${ENGINEERING_SOURCE_DIR}/include/db/replication
    ${ENGINEERING_SOURCE_DIR}/include/db/consistency
    ${ENGINEERING_SOURCE_DIR}/src
)

target_link_libraries(consistency_test PRIVATE
    db_consistency
    db_consensus
    db_replication
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

set_target_properties(consistency_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

include(GoogleTest)
gtest_discover_tests(consistency_test)
