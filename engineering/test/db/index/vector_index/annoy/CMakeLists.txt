# Annoy 索引测试

add_executable(annoy_test annoy_test.cpp)

target_link_libraries(annoy_test PRIVATE
    annoy
    gtest
    gtest_main
    Threads::Threads
)
