# RQ 量化器测试

add_executable(rq_test rq_test.cpp)

target_link_libraries(rq_test PRIVATE
    rq
    gtest
    gtest_main
    Threads::Threads
)