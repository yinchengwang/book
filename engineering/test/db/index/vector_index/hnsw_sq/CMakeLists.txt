# HNSW-SQ 混合索引测试

add_executable(hnsw_sq_test hnsw_sq_test.cpp)

target_link_libraries(hnsw_sq_test PRIVATE
    hnsw_sq
    gtest
    gtest_main
    Threads::Threads
)