# 工程层 Raft 模块 ctest smoke 测试
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(raft_test ${TEST_SOURCES})

target_include_directories(raft_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
)

target_link_libraries(raft_test PRIVATE
    db_consensus
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

set_target_properties(raft_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

include(GoogleTest)
gtest_discover_tests(raft_test)
