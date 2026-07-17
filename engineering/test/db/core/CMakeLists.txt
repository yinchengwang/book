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
