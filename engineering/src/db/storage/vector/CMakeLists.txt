# 向量持久化层
# src/db/storage/vector/CMakeLists.txt

# 独立的向量持久化库（基于段的持久化方案）
# 这是一套新的持久化接口，与现有的 vector_engine.c 是独立实现的

set(VECTOR_PERSIST_SOURCES
    vector_page.c
    vector_segment.c
    vector_index_persist.c
    vector_wal.c
    vector_buf.c
    vector_engine.c
    vec_page.c
    # 直接编译 hnsw_persist.c（从 RAG 模块）
    ${ENGINEERING_SOURCE_DIR}/rag/src/rag/persist/hnsw_persist.c
)

add_library(vector_persist STATIC ${VECTOR_PERSIST_SOURCES})

target_include_directories(vector_persist PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${ENGINEERING_SOURCE_DIR}/rag/include
)

# 链接必要的基础库
target_link_libraries(vector_persist PUBLIC
    db_core
    algo-prod
    vector_index
)
