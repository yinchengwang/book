# SIFT 召回率测试
add_executable(sift_recall_test sift_recall.c)

target_link_libraries(sift_recall_test
    PRIVATE
        project_includes
        vector_index
        diskann
        ivf
        heap_vector_store
        storage_backend_common
        algo-prod
        db_utils
)

set_target_properties(sift_recall_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# KD-Tree 召回率测试
add_executable(kd_tree_recall kd_tree_recall.c)
target_link_libraries(kd_tree_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod)
set_target_properties(kd_tree_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# IVF-Flat 召回率测试
add_executable(ivf_flat_recall ivf_flat_recall.c)
target_link_libraries(ivf_flat_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod)
set_target_properties(ivf_flat_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# IVF-PQ 召回率测试
add_executable(ivf_pq_recall ivf_pq_recall.c)
target_link_libraries(ivf_pq_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod)
set_target_properties(ivf_pq_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# NSW 召回率测试
add_executable(nsw_recall nsw_recall.c)
target_link_libraries(nsw_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod)
set_target_properties(nsw_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# faiss_ivf_compat 召回率测试
add_executable(faiss_ivf_compat_recall faiss_ivf_compat_recall.c)
target_link_libraries(faiss_ivf_compat_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod)
set_target_properties(faiss_ivf_compat_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ScaNN 召回率测试
add_executable(scann_recall scann_recall.c)
target_link_libraries(scann_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod)
set_target_properties(scann_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Ball Tree 召回率测试
add_executable(ball_tree_recall ball_tree_recall.c)
target_link_libraries(ball_tree_recall PRIVATE project_includes vector_index diskann ivf heap_vector_store storage_backend_common algo-prod)
set_target_properties(ball_tree_recall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# HNSW 召回率测试
add_executable(hnsw_recall_test hnsw_recall_test.c)
target_link_libraries(hnsw_recall_test PRIVATE project_includes vector_index vector_persist diskann ivf heap_vector_store storage_backend_common algo-prod)
set_target_properties(hnsw_recall_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
