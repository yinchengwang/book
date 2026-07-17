# ITQ 量化器测试

add_executable(itq_test itq_test.cpp)

target_link_libraries(itq_test PRIVATE
    itq
    gtest
    gtest_main
    Threads::Threads
)