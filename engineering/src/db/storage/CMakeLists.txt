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

# ========================================================================
# 各引擎实现（按 MMDB_ENABLE_* 开关条件编译）
# 注意：部分引擎（graph/doc/ts/spatial/yang）代码存在但有编译问题，
#       需要先修复才能启用。默认仅启用 KV 和 Vector。
# ========================================================================

# KV 引擎
if(MMDB_ENABLE_KV)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kv")
        add_subdirectory(kv)
    endif()
endif()

# 向量引擎
if(MMDB_ENABLE_VECTOR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vector")
        add_subdirectory(vector)
    endif()
endif()

# 图引擎（代码存在但有编译问题，暂不启用）
if(MMDB_ENABLE_GRAPH)
    message(STATUS "Graph engine: code exists but has build issues, not enabling")
endif()

# 文档引擎（代码存在但有编译问题，暂不启用）
if(MMDB_ENABLE_DOCUMENT)
    message(STATUS "Document engine: code exists but has build issues, not enabling")
endif()

# 时序引擎（代码存在但有编译问题，暂不启用）
if(MMDB_ENABLE_TIMESERIES)
    message(STATUS "Timeseries engine: code exists but has build issues, not enabling")
endif()

# 空间引擎（代码存在但有编译问题，暂不启用）
if(MMDB_ENABLE_SPATIAL)
    message(STATUS "Spatial engine: code exists but has build issues, not enabling")
endif()

# 树引擎（代码存在但有编译问题，暂不启用）
if(MMDB_ENABLE_TREE)
    message(STATUS "Tree engine: code exists but has build issues, not enabling")
endif()

# 流式引擎
if(MMDB_ENABLE_STREAM)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/stream")
        add_subdirectory(stream)
        message(STATUS "Stream engine: enabled")
    endif()
endif()

# 列存引擎（预留）
if(MMDB_ENABLE_COLUMNAR)
    message(STATUS "Columnar engine: not yet implemented")
endif()

# ========================================================================
# 组合 db_storage 主库
# ========================================================================

# 收集所有存在的 OBJECT 库
set(DB_STORAGE_OBJECTS "")

# 条件收集 OBJECT 库（仅收集可正常编译的引擎）
set(DB_STORAGE_SUBMODULES buffer page lock catalog wal rel mm)
if(MMDB_ENABLE_KV AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kv")
    list(APPEND DB_STORAGE_SUBMODULES kv)
endif()
if(MMDB_ENABLE_VECTOR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vector")
    list(APPEND DB_STORAGE_SUBMODULES vector)
endif()
if(MMDB_ENABLE_STREAM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/stream")
    list(APPEND DB_STORAGE_SUBMODULES stream)
endif()

foreach(subdir ${DB_STORAGE_SUBMODULES})
    if(TARGET storage_${subdir})
        list(APPEND DB_STORAGE_OBJECTS $<TARGET_OBJECTS:storage_${subdir}>)
    endif()
endforeach()

# vector_engine_lib 是静态库，单独处理
if(TARGET vector_engine_lib)
    target_link_libraries(vector_engine_lib PUBLIC algo-prod db_utils)
endif()

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
    # 链接 Hilbert 索引库（被 spatial 模块使用）
    if(TARGET index_hilbert)
        target_link_libraries(db_storage PUBLIC index_hilbert)
    endif()
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
