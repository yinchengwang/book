# engineering/test/CMakeLists.txt
# 工程层测试入口（仅 C++ 工程代码 + 数据库内核 + 独立应用）
# 双轨纪律：engineering/test 不应测 ds/、leetcode/、interview/ 库（这些已迁到 learning/）

cmake_minimum_required(VERSION 3.20)

# 启用测试
enable_testing()

# 工程层测试子目录（按子目录细分）
# 说明：S6 起移除 self_made/、leetcode/、interview/ 测试目录（被测库已迁移到 learning/）
add_subdirectory(algo)             # 算法测试（algo-prod）
add_subdirectory(self_made_cpp)   # C++ 工程代码（trie）
add_subdirectory(db)              # 数据库内核测试
add_subdirectory(kbase)            # kbase 知识库测试
add_subdirectory(db/consensus)   # Raft 共识模块测试（Phase11）
add_subdirectory(db/index/vector_index)  # 向量索引测试（S7 修复 algo → algo-prod 后重启）
add_subdirectory(apps)            # 独立应用测试（游戏、工具等）
add_subdirectory(fuzz)            # 模糊测试（A4）

# 创建一个聚合测试目标
add_custom_target(all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all engineering tests..."
    VERBATIM
)

# all_tests 目标依赖：每个测试 target 都自动加入（除了子目录 internal target）
set(_test_targets
    raft_test
    self_made_cpp_test
    test_db
    test_mvcc_test
    test_optimizer_test
    test_replication_test
    test_sql
    test_storage_all
    test_common_menu_test
    test_common_terminal_test
    test_concurrency
    test_hilbert
    test_mm_pool
    test_mvcc
    test_optimizer
    test_replication
    test_rest_api
    test_sla
    test_sql_parser
    test_storage_kv
    test_storage_kv_ttl
    test_storage_main
    test_storage_wal
    test_storage_buf
    test_vector_api
    test_vector_persist
    test_vector_query
    vector_delete_test
    vector_reranker_test
    test_fault_inject
    test_fuzz_page
    test_fuzz_sql
    test_fuzz_wal
    test_storage_kv_crash
    test_storage_heap_crash
    test_storage_btree_crash
)

foreach(_t IN LISTS _test_targets)
    if(TARGET ${_t})
        add_dependencies(all_tests ${_t})
    endif()
endforeach()
