# RAG Query Expansion 子模块
add_library(rag_query_expansion OBJECT
    query_expander.cpp
)
target_include_directories(rag_query_expansion PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
