# KD-Tree 索引测试

add_executable(kd_tree_test kd_tree_test.cpp)

target_link_libraries(kd_tree_test PRIVATE
    kd_tree
    gtest
    gtest_main
    Threads::Threads
)