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
# 不使用 gtest_discover_tests：无论 POST_BUILD 还是 PRE_TEST 模式，它都会**执行**
# raft_test.exe 来枚举用例；而该程序启动即 Access violation（既有运行时缺陷，
# 此前被构建失败掩盖——基线里 raft_test 根本没被构建出来）。
#   - POST_BUILD：构建期崩溃 → 整个 build 失败，把运行时缺陷伪装成构建失败
#   - PRE_TEST  ：ctest 枚举阶段崩溃 → 整个测试套件中断，其余测试也跑不了
# 改为单条 add_test：崩溃只让这一条失败，其余测试不受影响，缺陷如实暴露。
add_test(NAME raft_test COMMAND raft_test)
set_tests_properties(raft_test PROPERTIES LABELS "raft_test")
