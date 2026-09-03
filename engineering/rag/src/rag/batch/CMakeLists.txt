# engineering/rag/src/rag/batch/CMakeLists.txt

add_library(rag_batch STATIC
    batch_processor.cpp
    streaming_builder.cpp
)

target_include_directories(rag_batch PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(rag_batch PUBLIC
    rag_config
    rag_data
    rag_chunker
    rag_embedding
    rag_index
    rag_gpu
    Threads::Threads
)

# 条件链接 CUDA
if(RAG_HAS_CUDA)
    target_link_libraries(rag_batch PRIVATE CUDA::cudart)
endif()