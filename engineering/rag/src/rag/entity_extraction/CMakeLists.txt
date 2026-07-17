# RAG Entity Extraction 子模块
add_library(rag_entity_extraction OBJECT
    entity_extractor.cpp
)
target_include_directories(rag_entity_extraction PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
