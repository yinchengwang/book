# IVF-HNSW 混合索引测试

add_executable(ivf_hnsw_test ivf_hnsw_test.cpp)

target_link_libraries(ivf_hnsw_test PRIVATE
    ivf_hnsw
    gtest
    gtest_main
    Threads::Threads
)