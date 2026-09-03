# engineering/rag/src/rag/async/CMakeLists.txt

add_library(rag_async STATIC
    thread_pool.cpp
    async_pipeline.cpp
)

target_include_directories(rag_async PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(rag_async PUBLIC
    rag_pipeline
    rag_cache
    Threads::Threads
)