# 稀疏向量 + BM25 混合检索子模块
# src/db/storage/sparse/CMakeLists.txt

set(SPARSE_SOURCES
    sparse_vector.c
    bm25_index.c
    hybrid_retrieval.c
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
    target_sources(storage_sparse PRIVATE strndup_win32.c)
endif()

# db_utils 可能为空，改为可选链接
# target_link_libraries(storage_sparse PUBLIC db_utils)
