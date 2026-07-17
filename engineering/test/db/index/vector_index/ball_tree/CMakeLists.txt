# Ball-Tree 测试

add_executable(ball_tree_test ball_tree_test.cpp)

target_link_libraries(ball_tree_test PRIVATE
    ball_tree
    gtest
    gtest_main
    Threads::Threads
)