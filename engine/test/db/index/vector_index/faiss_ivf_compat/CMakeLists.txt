# Faiss IVF 兼容层测试

add_executable(faiss_ivf_compat_test faiss_ivf_compat_test.cpp)

target_link_libraries(faiss_ivf_compat_test PRIVATE
    faiss_ivf_compat
    gtest
    gtest_main
    Threads::Threads
)