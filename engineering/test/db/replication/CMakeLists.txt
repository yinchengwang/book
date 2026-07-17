# Replication 模块测试

add_executable(test_replication test_replication.cpp)
target_link_libraries(test_replication db_replication project_includes gtest gtest_main)

add_custom_target(test_replication_test
    COMMAND test_replication
    DEPENDS test_replication
)