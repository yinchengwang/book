# GPU 测试子目录

# GPU 向量索引测试
add_project_test(gpu_vector_index_test
    GPU_VECTOR_INDEX_TEST_SOURCES
)

set(GPU_VECTOR_INDEX_TEST_SOURCES
    gpu_vector_index_test.cpp
)

# GPU 性能基准测试
add_executable(gpu_benchmark gpu_benchmark.cpp)
target_link_libraries(gpu_benchmark PRIVATE
    project_includes
    algo-prod
)

# 链接 GPU 模块
target_link_libraries(gpu_vector_index_test PRIVATE
    project_includes
    algo-prod
)

target_link_libraries(gpu_benchmark PRIVATE
    project_includes
    algo-prod
)
