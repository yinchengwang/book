# 集成测试 CMake 配置

add_project_test(integration_test INTEGRATION_TEST_SRCS)
target_link_libraries(integration_test
    PRIVATE
        project_includes
        vector_index
        diskann
        ivf
        heap_vector_store
        storage_backend_common
        algo-prod
        db_utils
        faiss_hnsw
        hnsw
)
