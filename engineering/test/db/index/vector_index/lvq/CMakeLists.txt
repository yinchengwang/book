# LVQ 量化器测试

add_executable(lvq_test lvq_test.cpp)

target_link_libraries(lvq_test PRIVATE
    lvq
    gtest
    gtest_main
    Threads::Threads
)