# Concurrency 模块测试

add_executable(test_mvcc test_mvcc.cpp)
target_link_libraries(test_mvcc db_mvcc project_includes gtest gtest_main)

add_custom_target(test_mvcc_test
    COMMAND test_mvcc
    DEPENDS test_mvcc
)