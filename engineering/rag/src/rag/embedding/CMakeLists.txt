# RAG Embedding 子模块
add_library(rag_embedding OBJECT
    embedding.cpp
)
target_include_directories(rag_embedding PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
