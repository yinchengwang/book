# Modular RAG API 子模块
add_library(rag_modular_api OBJECT
    server.cpp
)
target_include_directories(rag_modular_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_link_libraries(rag_modular_api PRIVATE
    rag_modular_core
    rag_logger
    ws2_32
)
