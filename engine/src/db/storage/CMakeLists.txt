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
#   blob/     - Blob 存储引擎

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
# Blob 存储引擎
# ========================================================================

add_subdirectory(blob)

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

# 图引擎
if(MMDB_ENABLE_GRAPH)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/graph")
        add_subdirectory(graph)
    endif()
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
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/spatial")
        add_subdirectory(spatial)
        message(STATUS "Spatial engine: enabled")
    endif()
endif()

# 树引擎
if(MMDB_ENABLE_TREE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/yang")
        add_subdirectory(yang)
        message(STATUS "Tree engine: enabled")
    endif()
endif()

# 流式引擎
if(MMDB_ENABLE_STREAM)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/stream")
        add_subdirectory(stream)
        message(STATUS "Stream engine: enabled")
    endif()
endif()

# 列存引擎
if(MMDB_ENABLE_COLUMNAR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/columnar")
        add_subdirectory(columnar)
        message(STATUS "Columnar engine: enabled")
    endif()
endif()

# 时空引擎
if(MMDB_ENABLE_ST)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/st")
        add_subdirectory(st)
        message(STATUS "ST engine: enabled")
    endif()
endif()

# RDF 知识图谱引擎
if(MMDB_ENABLE_RDF)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rdf")
        add_subdirectory(rdf)
        message(STATUS "RDF engine: enabled")
    endif()
endif()

# 稀疏向量 + BM25 混合检索引擎
if(MMDB_ENABLE_SPARSE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sparse")
        add_subdirectory(sparse)
        message(STATUS "Sparse + BM25 hybrid engine: enabled")
    endif()
endif()

# ========================================================================
# 组合 db_storage 主库
# ========================================================================

# 收集所有存在的 OBJECT 库
set(DB_STORAGE_OBJECTS "")

# 条件收集 OBJECT 库（仅收集可正常编译的引擎）
set(DB_STORAGE_SUBMODULES buffer page lock catalog wal rel mm blob)
# yang（Task 10 起为独立静态库 mm_yang，不再编入聚合）
# kv（Task 7 起为独立静态库 mm_kv，不再编入聚合）
if(MMDB_ENABLE_VECTOR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vector")
    list(APPEND DB_STORAGE_SUBMODULES vector)
endif()
# graph（Task 9 起为独立静态库 mm_graph，不再编入聚合）
# spatial（Task 10 起为独立静态库 mm_spatial，不再编入聚合）
# stream（Task 11 起为独立静态库 mm_stream，不再编入聚合）
# columnar（Task 11 起为独立静态库 mm_columnar，不再编入聚合）
if(MMDB_ENABLE_ST AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/st")
    list(APPEND DB_STORAGE_SUBMODULES st)
endif()
if(MMDB_ENABLE_RDF AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rdf")
    list(APPEND DB_STORAGE_SUBMODULES rdf)
endif()
# sparse（Task 10 起为独立静态库 mm_sparse，不再编入聚合）

foreach(subdir ${DB_STORAGE_SUBMODULES})
    if(TARGET storage_${subdir})
        list(APPEND DB_STORAGE_OBJECTS $<TARGET_OBJECTS:storage_${subdir}>)
    endif()
endforeach()

# yang_parser（src/db/yang/，YANG 数据模型层）也是 OBJECT 库，其 obj 原本无人收集，
# 导致 yang_datastore_create / yang_model_create / yang_schema_find /
# yang_data_create_node 等 18 个符号在链接期缺失。
# src/db/CMakeLists.txt 已把 add_subdirectory(yang) 提前到 storage 之前，
# 故此处 yang_parser 目标已存在。
#
# 注意：不能用 list(APPEND DB_STORAGE_OBJECTS $<TARGET_OBJECTS:yang_parser>) ——
# add_library 的源文件位置不展开 $<TARGET_OBJECTS:...> 表达式（该表达式仅在
# target_sources 中有效），故这里改为在库定义完成后用 target_sources 追加。

# mm_vector 是静态库（包含向量引擎），单独处理
if(TARGET mm_vector)
    target_link_libraries(mm_vector PUBLIC algo-prod)
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
    # 链接 mm_vector（包含向量引擎 + HNSW 索引）
    if(TARGET mm_vector)
        target_link_libraries(db_storage PUBLIC mm_vector)
    endif()
    # 链接 Hilbert 索引库（被 spatial 模块使用）
    if(TARGET index_hilbert)
        target_link_libraries(db_storage PUBLIC index_hilbert)
    endif()
    # yang_parser（src/db/yang/）的 obj 用 target_sources 追加：
    # $<TARGET_OBJECTS:...> 表达式只在 target_sources 中有效，
    # 在 add_library 的源文件位置不会被展开。
    if(TARGET yang_parser)
        target_sources(db_storage PRIVATE $<TARGET_OBJECTS:yang_parser>)
    endif()
else()
    message(FATAL_ERROR "db_storage 没有任何源文件：检查 MMDB_MODALITY profile 与 storage 子目录")
endif()

# Task 7/9：独立模态静态库，聚合库以链接方式引入（不再收对象）
if(TARGET mm_kv)
    target_link_libraries(db_storage PUBLIC mm_kv)
endif()
if(TARGET mm_graph)
    target_link_libraries(db_storage PUBLIC mm_graph)
endif()
if(TARGET mm_spatial)
    target_link_libraries(db_storage PUBLIC mm_spatial)
endif()
if(TARGET mm_yang)
    target_link_libraries(db_storage PUBLIC mm_yang)
endif()
if(TARGET mm_sparse)
    target_link_libraries(db_storage PUBLIC mm_sparse)
endif()
if(TARGET mm_stream)
    target_link_libraries(db_storage PUBLIC mm_stream)
endif()
if(TARGET mm_columnar)
    target_link_libraries(db_storage PUBLIC mm_columnar)
endif()
