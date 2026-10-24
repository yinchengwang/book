# core 模块测试（guc, initdb, pg_ctl, db_server, vector_query）
add_project_test(guc test.cpp)

# 引擎注册表测试
add_executable(engine_registry_test engine_registry_test.cpp)
target_link_libraries(engine_registry_test PRIVATE db_core db_storage project_includes gtest gtest_main)

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

# 生成配置头测试（验证 multimodal_config.h 来自 build 树生成版）
# 注意：故意只链接 project_includes，不直接链接 multimodal_config_gen ——
# 本测试的目的就是证明生成头经由 project_includes 全局可达。
add_executable(test_multimodal_config test_multimodal_config.cpp)
target_link_libraries(test_multimodal_config
    PRIVATE
    project_includes
    gtest
    gtest_main
)
add_test(NAME MultimodalConfig COMMAND test_multimodal_config)

# 链接 db_core 库
target_link_libraries(guc PRIVATE db_core)
