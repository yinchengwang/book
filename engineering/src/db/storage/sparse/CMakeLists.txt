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
)

# db_utils 可能为空，改为可选链接
# target_link_libraries(storage_sparse PUBLIC db_utils)
