# Storage 模块测试

# KV 测试
add_executable(test_storage_kv kv.cpp)
target_link_libraries(test_storage_kv db_storage project_includes gtest gtest_main)

# 事务测试 - 暂时禁用 (依赖未启用的 txn 模块)
# add_executable(test_storage_txn txn.cpp)
# target_link_libraries(test_storage_txn db_storage project_includes gtest gtest_main)

# WAL 测试
add_executable(test_storage_wal wal.cpp)
target_link_libraries(test_storage_wal db_storage project_includes gtest gtest_main)

# 存储测试
add_executable(test_storage_main storage.cpp)
target_link_libraries(test_storage_main db_storage project_includes gtest gtest_main)

# 多模态存储测试 - 暂时禁用 (依赖未启用的 graph/ts 模块)
# add_executable(test_storage_mm mm_storage_test.cpp)
# target_link_libraries(test_storage_mm db_storage index_hilbert project_includes gtest gtest_main)

# 图引擎遍历测试 - 暂时禁用 (依赖未启用的 graph 模块)
# add_executable(test_storage_graph graph_traverse_test.cpp)
# target_link_libraries(test_storage_graph db_storage project_includes gtest gtest_main)

# 文档引擎测试 - 暂时禁用 (依赖未修复的 doc 模块)
# add_executable(test_storage_doc doc_engine_test.cpp)
# target_link_libraries(test_storage_doc db_storage project_includes gtest gtest_main)

# Yang 树引擎测试 - 暂时禁用 (依赖未启用的 yang 模块)
# add_executable(test_storage_yang yang_engine_test.cpp)
# target_link_libraries(test_storage_yang db_storage project_includes gtest gtest_main)

# 时序引擎测试 - 暂时禁用 (依赖未启用的 ts 模块)
# add_executable(test_storage_ts ts_engine_test.cpp)
# target_link_libraries(test_storage_ts db_storage project_includes gtest gtest_main)

# KV TTL 测试
add_executable(test_storage_kv_ttl kv_ttl_test.cpp)
target_link_libraries(test_storage_kv_ttl db_storage project_includes gtest gtest_main pthread)

# 性能基准测试 - 暂时禁用 (依赖未启用的 graph/ts 模块)
# add_executable(test_storage_benchmark storage_benchmark.cpp)
# target_link_libraries(test_storage_benchmark db_storage project_includes gtest gtest_main)

# Hilbert 索引测试
add_executable(test_hilbert hilbert_test.cpp)
target_link_libraries(test_hilbert index_hilbert project_includes gtest gtest_main)

# 空间引擎测试 - 暂时禁用 (依赖未启用的 spatial 模块)
# add_executable(test_storage_spatial spatial_engine_test.cpp)
# target_link_libraries(test_storage_spatial db_storage index_hilbert project_includes gtest gtest_main)

# 统一内存池测试
add_executable(test_mm_pool mm_pool_test.cpp)
target_link_libraries(test_mm_pool db_storage project_includes gtest gtest_main pthread)

# 并发安全测试
add_executable(test_concurrency test_concurrency.cpp)
target_link_libraries(test_concurrency project_includes gtest gtest_main pthread)

# 向量索引测试 - 暂时禁用 (链接问题)
# add_executable(test_vector_index test_vector_index.cpp)
# target_link_libraries(test_vector_index vector_engine_lib db_storage project_includes gtest gtest_main pthread)

# VecPage 向量页测试 - 暂时禁用 (链接问题)
# add_executable(test_vec_page vec_page_test.cpp)
# target_link_libraries(test_vec_page vector_engine_lib db_storage project_includes gtest gtest_main)

# 向量持久化层测试
add_executable(test_vector_persist vector_persist_test.cpp)
target_link_libraries(test_vector_persist vector_persist vector_index db_storage project_includes gtest gtest_main)

# 向量查询混合搜索内存安全测试
add_executable(test_vector_query_hybrid_memory vector_query_hybrid_memory_test.cpp)
target_link_libraries(test_vector_query_hybrid_memory vector_engine_lib db_storage project_includes gtest gtest_main)

# 向量 WAL 测试
add_executable(test_vector_wal vector_wal_test.cpp)
target_link_libraries(test_vector_wal vector_persist vector_index db_storage project_includes gtest gtest_main pthread)

# 链接 vector_index 库以提供 faiss_hnsw 和 hnsw_persist 函数
target_link_libraries(test_vector_wal vector_index)

# 注册测试
add_test(NAME test_vector_wal COMMAND test_vector_wal WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# 向量持久化基准测试 - 暂时禁用 (依赖未实现的 API)
# add_executable(test_vector_persist_bench vector_persist_benchmark.cpp)
# target_link_libraries(test_vector_persist_bench vector_persist vector_index db_storage project_includes gtest gtest_main)

# Hilbert 索引测试 (已在第 47-49 行定义)

# === A1: Buffer Pool 校验和验证测试 ===
add_executable(test_storage_buf buf_test.cpp)
target_link_libraries(test_storage_buf db_storage project_includes gtest gtest_main)

# === A2: Crash recovery tests (DISABLED - need WAL integration) ===
add_executable(test_storage_kv_crash kv_crash_test.cpp)
target_link_libraries(test_storage_kv_crash db_storage project_includes gtest gtest_main)
add_executable(test_storage_heap_crash heap_crash_test.cpp)
target_link_libraries(test_storage_heap_crash db_storage project_includes gtest gtest_main)
add_executable(test_storage_btree_crash btree_crash_test.cpp)
target_link_libraries(test_storage_btree_crash db_storage project_includes gtest gtest_main)

# 图引擎 storage_ops_t 接入测试（Task 2.11）
add_executable(test_graph_engine_ops test_graph_engine_ops.cpp)
target_link_libraries(test_graph_engine_ops db_storage project_includes gtest gtest_main)

# 空间引擎 storage_ops_t 接入测试（Task 2.12）
add_executable(test_spatial_engine_ops test_spatial_engine_ops.cpp)
target_link_libraries(test_spatial_engine_ops db_storage project_includes gtest gtest_main)

# 所有 Storage 测试汇总
add_custom_target(test_storage_all
    COMMAND test_storage_kv
    COMMAND test_storage_wal
    COMMAND test_storage_main
    COMMAND test_storage_kv_ttl
    COMMAND test_mm_pool
    COMMAND test_concurrency
    COMMAND test_hilbert
    COMMAND test_graph_engine_ops
    COMMAND test_spatial_engine_ops
)
