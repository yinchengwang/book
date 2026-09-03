# Milvus IVF 测试

add_executable(milvus_ivf_test milvus_ivf_test.cpp)

target_link_libraries(milvus_ivf_test PRIVATE
    milvus_ivf
    gtest
    gtest_main
    Threads::Threads
)