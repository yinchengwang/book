# HNSW 量化测试 (SQ/PQ/LVQ/RQ)

add_executable(test_faiss_hnsw_quantize test_faiss_hnsw_quantize.c)

target_include_directories(test_faiss_hnsw_quantize PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${ENGINEERING_SOURCE_DIR}/src/db/index/vector_index
    ${ENGINEERING_SOURCE_DIR}/src/db/index/vector_index/faiss_hnsw
)

target_link_libraries(test_faiss_hnsw_quantize PRIVATE
    faiss_hnsw
    vector_index
    algo-prod
    project_includes
)

set_target_properties(test_faiss_hnsw_quantize PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)