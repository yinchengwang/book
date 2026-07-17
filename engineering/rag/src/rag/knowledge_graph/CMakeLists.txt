# RAG Knowledge Graph 子模块
add_library(rag_knowledge_graph OBJECT
    knowledge_graph.cpp
)
target_include_directories(rag_knowledge_graph PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
