# RAG Fusion 子模块
add_library(rag_fusion OBJECT
    adaptive_rrf.cpp
)
target_include_directories(rag_fusion PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)