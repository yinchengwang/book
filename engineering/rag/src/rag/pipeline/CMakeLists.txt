# engineering/rag/src/rag/pipeline/CMakeLists.txt

add_library(rag_pipeline STATIC
    pipeline.cpp
)

target_include_directories(rag_pipeline PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(rag_pipeline PUBLIC
    rag_retrieval
    rag_reranker
    rag_query_expansion
    rag_cache
    rag_config
    rag_logger
    Threads::Threads
)