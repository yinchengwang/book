# VDB Wave 3 端到端集成测试
add_executable(vdb_e2e_test vdb_e2e_test.cpp)
target_link_libraries(vdb_e2e_test
    PRIVATE
        vector_api
        db_core
        project_includes
        GTest::gtest_main
)

# VDB Wave 3 压力测试
add_executable(vdb_stress_test vdb_stress_test.cpp)
target_link_libraries(vdb_stress_test
    PRIVATE
        vector_api
        db_core
        project_includes
        GTest::gtest_main
)

# VDB Wave 3 RAG 端到端测试
# 依赖 rag 库（当前禁用），暂不构建
# add_executable(vdb_rag_e2e_test vdb_rag_e2e_test.cpp)
# target_link_libraries(vdb_rag_e2e_test
#     PRIVATE
#         rag
#         project_includes
#         GTest::gtest_main
# )

# VDB Wave 3 混沌测试
add_executable(vdb_chaos_test vdb_chaos_test.cpp)
target_link_libraries(vdb_chaos_test
    PRIVATE
        vector_api
        db_core
        project_includes
        GTest::gtest_main
)

# 向量操作测试
# add_executable(test_vector_ops_integration vector_ops_test.cpp)
# target_link_libraries(test_vector_ops_integration
#     PRIVATE
#         vector_api
#         db_core
#         project_includes
#         GTest::gtest_main
# )

# 端到端测试（原有）
# add_executable(test_e2e_integration e2e_test.cpp)
# target_link_libraries(test_e2e_integration
#     PRIVATE
#         vector_api
#         db_core
#         project_includes
#         GTest::gtest_main
# )
