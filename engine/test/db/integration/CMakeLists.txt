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

# P1-A: Vector WAL 恢复独立单元测试
add_executable(vdb_wal_recovery_test vdb_wal_recovery_test.cpp)
target_link_libraries(vdb_wal_recovery_test
    PRIVATE
        vector_api
        db_core
        project_includes
        GTest::gtest_main
)

# P1-B: Vector CheckpointManager 独立单元测试
# P1-B Task 7：新增 BackgroundCheckpointAdvances 测试需要直接调用
#             vector_wal_get_checkpoint_info，故链接 mm_vector 库。
add_executable(vdb_checkpoint_test vdb_checkpoint_test.cpp)
target_link_libraries(vdb_checkpoint_test
    PRIVATE
        vector_api
        mm_vector   # P1-B Task 7：vector_wal_get_checkpoint_info
        db_core
        project_includes
        GTest::gtest_main
        Threads::Threads
)

# P1-C: chaos_e2e 端到端混沌测试（TDD 红阶段）
# 5 用例：KillDuringCheckpoint / KillDuringReplay / PartialWriteRecovery /
#        ShortIntervalStress / ConcurrentSaveCrash
# 依赖 mm_vector（partial write 测试直接 fopen WAL 文件）+ vector_api
# 同时使用项目统一线程库（pthread/SRWLOCK 抽象）
add_executable(chaos_e2e_test chaos_e2e_test.cpp)
target_link_libraries(chaos_e2e_test
    PRIVATE
        vector_api
        mm_vector   # P1-C: PartialWriteRecovery 直接操作 vector_wal_0.log
        db_core
        project_includes
        GTest::gtest_main
        Threads::Threads
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
