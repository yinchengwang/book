# RAG Engine 子模块
add_library(rag_engine OBJECT
    engine.cpp
    graph_extension.cpp
)
target_include_directories(rag_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_link_libraries(rag_engine PUBLIC
    rag_config
    rag_data
    rag_chunker
    rag_parser
    rag_index
    rag_retrieval
    rag_logger
    rag_knowledge_graph
    rag_entity_extraction
    rag_graph_retrieval
)
