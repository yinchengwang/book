# RAG Retrieval 子模块
add_library(rag_retrieval OBJECT
    retriever.cpp
    enhanced_retriever.cpp
)
target_include_directories(rag_retrieval PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
