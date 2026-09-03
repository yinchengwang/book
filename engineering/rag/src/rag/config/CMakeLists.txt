# RAG Config 子模块
add_library(rag_config OBJECT
    config.cpp
)
target_include_directories(rag_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
