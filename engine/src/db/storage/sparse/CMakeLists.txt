# 稀疏向量 + BM25 混合检索子模块
# src/db/storage/sparse/CMakeLists.txt

set(SPARSE_SOURCES
    sparse_vector.c
    hybrid_retrieval.c
)

# Task 8：bm25_index.c 从 db_storage 聚合中分离。
# 它与 vector_index/BM25（db_bm25，rest_api/handlers 实际使用的实现）
# 定义了同名但签名不同的 bm25_index_* 符号（bm25_index_create(config) vs
# bm25_index_create(void)）；两者同时被拉入一个可执行会 multiple definition。
# db_bm25 经 api 链接提供 rest_api 所需；本对象库仅供直接链接它的
# test_sparse / test_vector_index 使用。
set(SPARSE_BM25_SOURCES
    bm25_index.c
)

# 构建为 OBJECT 库，供 db_storage 静态库链接
add_library(storage_sparse OBJECT ${SPARSE_SOURCES})

target_include_directories(storage_sparse PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/third_part/uthash/src
)

# T12：bm25_index.c 使用 POSIX strndup；MinGW 无原生实现。加 -D_GNU_SOURCE 不够，
# 需提供静态回退（Windows 下 _strdup 截断版本）。
if(WIN32)
    target_compile_definitions(storage_sparse PRIVATE _CRT_SECURE_NO_WARNINGS)
    # strndup_win32.c 归 storage_sparse_bm25（bm25_index.c 是唯一使用方；
    # 两库都编会令同时链接两者的 test_sparse 报 multiple definition）
endif()

add_library(storage_sparse_bm25 OBJECT ${SPARSE_BM25_SOURCES})
target_include_directories(storage_sparse_bm25 PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/third_part/uthash/src
)
if(WIN32)
    target_compile_definitions(storage_sparse_bm25 PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_sources(storage_sparse_bm25 PRIVATE strndup_win32.c)
endif()

# db_utils 可能为空，改为可选链接
# target_link_libraries(storage_sparse PUBLIC db_utils)
