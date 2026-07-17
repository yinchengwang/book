# RAG CLI 子模块
add_library(rag_cli_objects OBJECT
    cli.cpp
)
target_include_directories(rag_cli_objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
