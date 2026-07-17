# HNSW-PQ 混合索引测试

file(GLOB HNSW_PQ_TEST_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

add_executable(hnsw_pq_test ${HNSW_PQ_TEST_SOURCES})

target_include_directories(hnsw_pq_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
)

target_link_libraries(hnsw_pq_test PRIVATE
    hnsw_pq
    pq
    heap_vector_store
    storage_backend_memory
    db_storage
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(hnsw_pq_test
    PROPERTIES LABELS "hnsw_pq;vector_index"
)