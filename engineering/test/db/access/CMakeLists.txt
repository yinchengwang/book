# Access Method 测试（Index AM 框架）
add_executable(test_amapi test_amapi.cpp)
target_link_libraries(test_amapi access_amapi project_includes gtest gtest_main)