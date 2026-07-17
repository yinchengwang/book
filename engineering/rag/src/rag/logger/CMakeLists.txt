# RAG Logger 子模块
add_library(rag_logger OBJECT
    logger.cpp
)
target_include_directories(rag_logger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
