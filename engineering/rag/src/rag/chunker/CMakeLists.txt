# RAG Chunker 子模块
add_library(rag_chunker OBJECT
    chunker.cpp
    semantic_chunker.cpp
)
target_include_directories(rag_chunker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
