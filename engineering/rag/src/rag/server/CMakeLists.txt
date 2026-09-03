# RAG Server 子模块
add_library(rag_server OBJECT
    server.cpp
)
target_include_directories(rag_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_link_libraries(rag_server PUBLIC
    ws2_32
)
