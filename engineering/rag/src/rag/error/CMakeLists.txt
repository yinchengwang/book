# RAG Error 子模块
add_library(rag_error OBJECT
    error.cpp
    retry.cpp
)
target_include_directories(rag_error PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
