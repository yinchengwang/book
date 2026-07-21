# Storage 模块测试

# BTree 页面格式测试（Task W1.1）
add_executable(test_btree btree_test.cpp)
target_link_libraries(test_btree project_includes gtest gtest_main)

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

# Yang 树引擎 storage_ops_t 接入测试（Task 2.15）
add_executable(test_yang_engine_ops test_yang_engine_ops.cpp)
target_link_libraries(test_yang_engine_ops db_storage project_includes gtest gtest_main)

# 向量引擎空集合建索引测试（Task 2.20）
add_executable(test_vector_build_empty test_vector_build_empty.cpp)
target_link_libraries(test_vector_build_empty db_storage project_includes gtest gtest_main)

# 向量引擎 active_index_type 测试（Task 2.21）
add_executable(test_vector_index_type test_vector_index_type.cpp)
target_link_libraries(test_vector_index_type db_storage project_includes gtest gtest_main)

# BTree 页面分裂测试（Task W1.2）
add_executable(test_btree_split btree_split_test.cpp)
target_link_libraries(test_btree_split db_storage project_includes gtest gtest_main)
add_test(NAME test_btree_split COMMAND test_btree_split)

# BTree 内部节点分裂测试（Task W1.3）
add_executable(test_btree_split_internal btree_split_internal_test.cpp)
target_link_libraries(test_btree_split_internal db_storage project_includes gtest gtest_main)
add_test(NAME test_btree_split_internal COMMAND test_btree_split_internal)

# BTree 并发 Latch 测试（Task W1.4）
add_executable(test_btree_concurrent btree_concurrent_test.cpp)
target_link_libraries(test_btree_concurrent db_storage project_includes gtest gtest_main pthread)
add_test(NAME test_btree_concurrent COMMAND test_btree_concurrent)

# CLOG 持久化测试（Task W1.5）
add_executable(test_clog clog_test.cpp)
target_link_libraries(test_clog db_storage project_includes gtest gtest_main)
add_test(NAME test_clog COMMAND test_clog WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Heap 元组头改造测试（Task W2.1）
add_executable(test_heap_tuple heap_tuple_test.cpp)
target_link_libraries(test_heap_tuple db_storage project_includes gtest gtest_main)
add_test(NAME test_heap_tuple COMMAND test_heap_tuple)

# Heap 更新版本链测试（Task W2.2）
add_executable(test_heap_update heap_update_test.cpp)
target_link_libraries(test_heap_update db_storage project_includes gtest gtest_main pthread)
add_test(NAME test_heap_update COMMAND test_heap_update)

# Heap 删除标记测试（Task W2.3）
add_executable(test_heap_delete heap_delete_test.cpp)
target_link_libraries(test_heap_delete db_storage project_includes gtest gtest_main)
add_test(NAME test_heap_delete COMMAND test_heap_delete)

# HOT 堆内元组测试（Task W2.4）
add_executable(test_hot hot_test.cpp)
target_link_libraries(test_hot db_storage project_includes gtest gtest_main)
add_test(NAME test_hot COMMAND test_hot)

# VACUUM 死元组清理测试（Task W2.5）
add_executable(test_vacuum_page vacuum_test.cpp)
target_link_libraries(test_vacuum_page db_storage project_includes gtest gtest_main)
add_test(NAME test_vacuum_page COMMAND test_vacuum_page)

# VACUUM 索引清理 + Freeze 测试（Task W2.6/W2.7）
add_executable(test_vacuum_index vacuum_index_freeze_test.cpp)
target_link_libraries(test_vacuum_index db_storage project_includes gtest gtest_main)
add_test(NAME test_vacuum_index COMMAND test_vacuum_index)

# TOAST 大值存储测试（Task W3.1）
add_executable(test_toast toast_test.cpp)
target_link_libraries(test_toast db_storage project_includes gtest gtest_main)
add_test(NAME test_toast COMMAND test_toast)

# 预读测试（Task W3.2）
add_executable(test_readahead readahead_test.cpp)
target_link_libraries(test_readahead db_storage project_includes gtest gtest_main)
add_test(NAME test_readahead COMMAND test_readahead)

# ANALYZE/序列/planner 统计测试（W3.3-W3.5）
add_executable(test_analyze_seq analyze_sequence_test.cpp)
target_link_libraries(test_analyze_seq db_storage db_tools_vacuum project_includes gtest gtest_main)
add_test(NAME test_analyze_seq COMMAND test_analyze_seq)

# 外键/CHECK 约束测试（W3.6-W3.7）
add_executable(test_foreign_key foreign_key_check_test.cpp)
target_link_libraries(test_foreign_key db_storage project_includes gtest gtest_main)
add_test(NAME test_foreign_key COMMAND test_foreign_key)

# Double Write 测试（W3.8）
add_executable(test_double_write double_write_test.cpp)
target_link_libraries(test_double_write db_storage project_includes gtest gtest_main)
add_test(NAME test_double_write COMMAND test_double_write)

# 两阶段提交 2PC 测试（W5.1）
add_executable(test_twophase twophase_test.cpp)
target_link_libraries(test_twophase db_storage project_includes gtest gtest_main)
add_test(NAME test_twophase COMMAND test_twophase)

# 2PC 崩溃恢复测试（W5.2）
add_executable(test_twophase_recovery twophase_recovery_test.cpp)
target_link_libraries(test_twophase_recovery db_storage project_includes gtest gtest_main)
add_test(NAME test_twophase_recovery COMMAND test_twophase_recovery WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# 子事务/保存点测试（W5.3）
add_executable(test_savepoint txn_savepoint_test.cpp)
target_link_libraries(test_savepoint db_storage project_includes gtest gtest_main)
add_test(NAME test_savepoint COMMAND test_savepoint WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# 所有 Storage 测试汇总

# 所有 Storage 测试汇总
add_custom_target(test_storage_all
    COMMAND test_btree
    COMMAND test_storage_kv
    COMMAND test_storage_wal
    COMMAND test_storage_main
    COMMAND test_storage_kv_ttl
    COMMAND test_mm_pool
    COMMAND test_concurrency
    COMMAND test_hilbert
    COMMAND test_graph_engine_ops
    COMMAND test_spatial_engine_ops
    COMMAND test_yang_engine_ops
)
