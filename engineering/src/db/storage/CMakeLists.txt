# 存储引擎子模块
# 参考 PostgreSQL storage + access 架构
#
# 子模块：
#   buffer/   - Buffer Pool
#   page/     - 页面管理
#   catalog/  - 系统目录
#   wal/      - 预写日志
#   txn/      - 事务管理
#   rel/      - Relation 管理
#   access/   - 访问方法（heap, btree）
#   mm/       - 多模态存储管理
#   kv/       - KV 存储引擎
#   vector/   - 向量存储引擎
#   graph/    - 图存储引擎
#   doc/      - 文档存储引擎
#   spatial/  - 空间存储引擎
#   ts/       - 时序存储引擎
#   yang/     - 树存储引擎

# ========================================================================
# 基础存储模块（页面/缓存层）
# ========================================================================

add_subdirectory(buffer)
add_subdirectory(page)
add_subdirectory(lock)

# ========================================================================
# 系统目录和事务
# ========================================================================

add_subdirectory(catalog)
add_subdirectory(wal)
# txn 模块暂时禁用 - 存在 TxnContext 未定义问题，待修复
# add_subdirectory(txn)
add_subdirectory(rel)

# ========================================================================
# 访问方法
# ========================================================================

add_subdirectory(access)

# ========================================================================
# 多模态存储管理
# ========================================================================

add_subdirectory(mm)

# CDC 订阅模块（暂时禁用，待实现）
# add_subdirectory(cdc)

# ========================================================================
# 各引擎实现
# ========================================================================

add_subdirectory(kv)
add_subdirectory(vector)
add_subdirectory(graph)
add_subdirectory(spatial)
# doc/ts/yang 模块需手动启用
# add_subdirectory(doc)
# add_subdirectory(ts)
add_subdirectory(yang)

# ========================================================================
# 组合 db_storage 主库
# ========================================================================

# 收集所有存在的 OBJECT 库
set(DB_STORAGE_OBJECTS "")

# 条件收集 OBJECT 库（Task 2.12 添加 spatial 子模块）
foreach(subdir buffer page lock catalog wal rel mm kv vector graph spatial yang)
    if(TARGET storage_${subdir})
        list(APPEND DB_STORAGE_OBJECTS $<TARGET_OBJECTS:storage_${subdir}>)
    endif()
endforeach()

# vector_engine_lib 是静态库别名（指向 vector_persist），已在 vector/CMakeLists.txt 中定义
# 这里跳过 target_link_libraries，避免修改 ALIAS 目标

# Hilbert 索引库是静态库，需要链接
if(TARGET index_hilbert)
    # Hilbert 库会被 spatial 模块链接，这里不需要重复
endif()

# 收集 access 子目录的 OBJECT 库
if(TARGET storage_access_heap)
    list(APPEND DB_STORAGE_OBJECTS $<TARGET_OBJECTS:storage_access_heap>)
endif()
if(TARGET storage_access_btree)
    list(APPEND DB_STORAGE_OBJECTS $<TARGET_OBJECTS:storage_access_btree>)
endif()

# 组合为静态库
if(DB_STORAGE_OBJECTS)
    add_library(db_storage STATIC ${DB_STORAGE_OBJECTS})
    target_include_directories(db_storage PUBLIC
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
    )
    target_link_libraries(db_storage PUBLIC db_core)
    # 链接 vector_engine_lib（包含 HNSW 索引）
    if(TARGET vector_engine_lib)
        target_link_libraries(db_storage PUBLIC vector_engine_lib)
    endif()
    # 链接 Hilbert 索引库（被 spatial 模块使用，Task 2.12 修复）
    target_link_libraries(db_storage PUBLIC index_hilbert)
else()
    # 如果没有 OBJECT 库，创建空库作为占位
    add_library(db_storage STATIC IMPORTED)
    set_target_properties(db_storage PROPERTIES IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/src/db/libdb_storage.a)
    if(TARGET vector_engine_lib)
        target_link_libraries(db_storage PUBLIC vector_engine_lib)
    endif()
    if(TARGET index_hilbert)
        target_link_libraries(db_storage PUBLIC index_hilbert)
    endif()
endif()
