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

# Task 10：sparse 拆为独立静态库（不实现 storage_ops_t，无引擎注册；
# 提供稀疏向量 + BM25 混合检索 API）。依赖闭包：sparse -> vector。
add_library(mm_sparse STATIC ${SPARSE_SOURCES})

target_include_directories(mm_sparse PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/third_part/uthash/src
)
# nm 实测仅依赖 mm_base（logf）；白名单边 sparse -> vector
target_link_libraries(mm_sparse PUBLIC mm_base mm_vector)
set_target_properties(mm_sparse PROPERTIES FOLDER "ModalityEngines")

# T12：bm25_index.c 使用 POSIX strndup；MinGW 无原生实现，提供静态回退
# （strndup_win32.c 与 _CRT_SECURE_NO_WARNINGS 定义见下方 storage_sparse_bm25
# 的 WIN32 分支；bm25_index.c 是唯一使用方，两库都编会令同时链接两者的
# test_sparse 报 multiple definition）

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
