# SDK 内存上下文兼容层测试构建配置

# MemoryContext 兼容层测试（Task 7）
add_project_test(mmdb_memctx_test VAR)
target_link_libraries(mmdb_memctx_test PRIVATE mmsdk project_includes)

# mmdb_t 根上下文集成测试（Task 8）
add_project_test(mmdb_root_context_test VAR)
target_link_libraries(mmdb_root_context_test PRIVATE mmsdk project_includes)

# 请求级内存作用域测试（Task 9）
add_project_test(mmdb_request_scope_test VAR)
target_link_libraries(mmdb_request_scope_test PRIVATE mmsdk project_includes)

# Task 11：Core 模块 MemoryContext 迁移测试
add_project_test(sdk_core_memctx_test VAR)
target_link_libraries(sdk_core_memctx_test PRIVATE mmsdk project_includes)

# Task 11：Vectors 模块 MemoryContext 迁移测试
add_project_test(sdk_vectors_memctx_test VAR)
target_link_libraries(sdk_vectors_memctx_test PRIVATE mmsdk project_includes)

# Task 12：内存泄漏检测测试
add_project_test(memory_leak_detection_test VAR)
target_link_libraries(memory_leak_detection_test PRIVATE mmsdk project_includes)
