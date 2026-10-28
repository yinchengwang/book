# SDK 内存上下文兼容层测试构建配置
#
# 每个目标只编自己的源文件（SOURCES 显式指定）。
# 原因：add_project_test 的默认行为是 GLOB 当前目录所有 .cpp/.c，
# 在本目录（6 源 6 目标）会让每个源文件被编进全部 6 个目标，
# 同一 gtest 用例在 ctest 中注册 6 份。6 个副本共用固定数据文件
# （test_leak_detection.db / test_root_context.db / test_request_scope.db），
# 并行执行时互相踩踏，表现为随机失败。
#
# 注意：改为显式源后，ctest 用例总数会下降（去重 6 倍），
# 这是去重而非丢失覆盖——每个用例仍恰好执行一次。

# MemoryContext 兼容层测试（Task 7）
add_project_test(mmdb_memctx_test VAR
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mmdb_memctx_test.cpp)
target_link_libraries(mmdb_memctx_test PRIVATE mmsdk project_includes)

# mmdb_t 根上下文集成测试（Task 8）
add_project_test(mmdb_root_context_test VAR
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mmdb_root_context_test.cpp)
target_link_libraries(mmdb_root_context_test PRIVATE mmsdk project_includes)

# 请求级内存作用域测试（Task 9）
add_project_test(mmdb_request_scope_test VAR
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mmdb_request_scope_test.cpp)
target_link_libraries(mmdb_request_scope_test PRIVATE mmsdk project_includes)

# Task 11：Core 模块 MemoryContext 迁移测试
add_project_test(sdk_core_memctx_test VAR
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/sdk_core_memctx_test.cpp)
target_link_libraries(sdk_core_memctx_test PRIVATE mmsdk project_includes)

# Task 11：Vectors 模块 MemoryContext 迁移测试
add_project_test(sdk_vectors_memctx_test VAR
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/sdk_vectors_memctx_test.cpp)
target_link_libraries(sdk_vectors_memctx_test PRIVATE mmsdk project_includes)

# Task 12：内存泄漏检测测试
add_project_test(memory_leak_detection_test VAR
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/memory_leak_detection_test.cpp)
target_link_libraries(memory_leak_detection_test PRIVATE mmsdk project_includes)
