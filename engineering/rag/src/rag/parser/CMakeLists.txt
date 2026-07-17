# RAG Parser 子模块
add_library(rag_parser OBJECT
    parser.cpp
)
target_include_directories(rag_parser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
