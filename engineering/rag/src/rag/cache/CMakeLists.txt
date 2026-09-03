# engineering/rag/src/rag/cache/CMakeLists.txt

add_library(rag_cache STATIC
    lru_cache.cpp
    semantic_cache.cpp
)

target_include_directories(rag_cache PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(rag_cache PUBLIC
    rag_embedding
    rag_config
    Threads::Threads
)