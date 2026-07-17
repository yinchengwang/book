# RAG HNSW Persist 子模块
add_library(rag_persist OBJECT
    hnsw_persist.c
)
target_include_directories(rag_persist PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..  # for algo-prod headers
)
