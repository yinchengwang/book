# Multi-Probe LSH 测试

add_executable(lsh_multiprobe_test lsh_multiprobe_test.cpp)

target_link_libraries(lsh_multiprobe_test PRIVATE
    lsh_multiprobe
    gtest
    gtest_main
    Threads::Threads
)