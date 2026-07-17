# ScaNN 测试

add_executable(scann_test scann_test.cpp)

target_link_libraries(scann_test PRIVATE
    scann
    gtest
    gtest_main
    Threads::Threads
)