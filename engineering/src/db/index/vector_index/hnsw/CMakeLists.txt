# hnsw 模块：持久化版 HNSW 索引（基于 pgVector 页面管理风格）
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS *.c)
add_library(hnsw STATIC ${SRC})
target_include_directories(hnsw PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
)
target_link_libraries(hnsw PRIVATE algo-prod project_includes faiss_hnsw)