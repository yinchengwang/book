# RAG Reranker 子模块
add_library(rag_reranker OBJECT
    reranker.cpp
)
target_include_directories(rag_reranker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
