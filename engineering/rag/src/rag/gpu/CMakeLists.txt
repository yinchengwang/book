# engineering/rag/src/rag/gpu/CMakeLists.txt

add_library(rag_gpu STATIC
    gpu_config.cpp
)

target_include_directories(rag_gpu PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 检查 CUDA
find_package(CUDA QUIET)
if(CUDA_FOUND)
    target_compile_definitions(rag_gpu PUBLIC RAG_HAS_CUDA=1)
    target_link_libraries(rag_gpu PRIVATE CUDA::cuda_driver CUDA::cudart)
    message(STATUS "GPU: CUDA found, GPU acceleration enabled")
else()
    target_compile_definitions(rag_gpu PUBLIC RAG_HAS_CUDA=0)
    message(STATUS "GPU: CUDA not found, using CPU fallback")
endif()
