# SDK 内存上下文兼容层测试构建配置

# MemoryContext 兼容层测试
add_project_test(mmdb_memctx_test VAR)
target_link_libraries(mmdb_memctx_test PRIVATE mmsdk project_includes)
