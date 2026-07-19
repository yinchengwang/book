# core 模块测试（guc, initdb, pg_ctl, db_server, vector_query）
add_project_test(guc test.cpp)

# 向量查询执行器测试
add_executable(test_vector_query vector_query_test.cpp)
target_link_libraries(test_vector_query
    PRIVATE
    db_core
    vector_index
    ivf
    algo-prod
    project_includes
    gtest
    gtest_main
)

# 多模态查询测试（过滤表达式 + 结果融合）
add_executable(multimodal_test multimodal_test.cpp)
target_link_libraries(multimodal_test
    PRIVATE
    db_core
    vector_api
    algo-prod
    project_includes
    gtest
    gtest_main
)

# 链接 db_core 库
target_link_libraries(guc PRIVATE db_core)

# === A3: 故障注入测试 ===
add_executable(test_fault_inject fault_inject_test.cpp)
target_link_libraries(test_fault_inject db_core project_includes gtest gtest_main)

# === 备份恢复测试 ===
# 注意：备份测试存在循环依赖问题（db_core <-> db_storage），暂时禁用
# add_executable(test_backup backup_test.cpp)
# target_link_libraries(test_backup PRIVATE
#     project_includes
#     gtest
#     gtest_main
# )
# GCC 在 Windows 上需要 --start-group/--end-group 解决循环依赖
# target_link_options(test_backup PRIVATE "LINKER:--start-group")
# target_link_libraries(test_backup PRIVATE db_storage db_core)
# target_link_options(test_backup PRIVATE "LINKER:--end-group")
