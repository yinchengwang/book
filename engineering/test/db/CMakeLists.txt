# 数据库内核测试
# 目录结构与 src/db/ 完全镜像

# 添加子目录（每个子模块有自己的 CMakeLists.txt）
add_subdirectory(parser)  # 解析器测试
add_subdirectory(sql)
add_subdirectory(storage)
# concurrency 模块已移除（合并到 storage/txn/mvcc.c）
# add_subdirectory(concurrency)
# constraints 模块源代码已迁移到 validator，测试暂时跳过
# add_subdirectory(constraints)
# add_subdirectory(optimizer)  # 优化器测试在 sql/ 目录下
add_subdirectory(replication)
add_subdirectory(cli)
add_subdirectory(core)
add_subdirectory(api)
add_subdirectory(executor)
add_subdirectory(protocol)
add_subdirectory(integration)
add_subdirectory(benchmark)
add_subdirectory(tools)
add_subdirectory(index)
add_subdirectory(bgworker)
add_subdirectory(access)

# 订阅/对账/老化模块测试
add_executable(test_sla sla_test.cpp)
target_link_libraries(test_sla
    subscription_lib
    ledger_lib
    aging_lib
    db_core
    project_includes
    gtest
    gtest_main
)

# 全量测试目标
add_custom_target(db_all_tests
    COMMAND echo "Running all DB tests..."
    DEPENDS
        test_sql
        test_storage
        # test_mvcc_test  # 已迁移到 storage/txn/mvcc.c
        # test_constraints_test  # 已跳过
        # test_optimizer_test  # 已迁移到 test_sql
        test_replication_test
        test_cli_test
        test_guc
        test_sla
)

# 便捷别名
add_custom_target(test_db
    COMMAND echo "Use: test_sql, test_storage, etc."
)