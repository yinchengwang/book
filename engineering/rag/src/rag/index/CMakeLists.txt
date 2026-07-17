# RAG Index 子模块
add_library(rag_index OBJECT
    vector_index.cpp
    bm25_index.cpp
)
target_include_directories(rag_index PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
