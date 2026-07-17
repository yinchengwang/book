# RAG Metrics 子模块
add_library(rag_metrics OBJECT
    metrics.cpp
)
target_include_directories(rag_metrics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
