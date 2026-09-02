# 工程层 Raft 模块 ctest smoke 测试
file(GLOB CPP_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB C_TEST_SOURCES CONFIGURE_DEPENDS "*.c")

# C 测试文件列表
set(C_TEST_FILES
    raft_transport_test.c
)

add_executable(raft_test ${CPP_TEST_SOURCES} ${C_TEST_FILES})

target_include_directories(raft_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/consensus
    ${ENGINEERING_SOURCE_DIR}/include/db/distributed
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
