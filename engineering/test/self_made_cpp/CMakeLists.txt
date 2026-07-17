# 头文件路径 会将目录下的子文件也包含
include_directories(${ENGINEERING_SOURCE_DIR}/include)

# 添加测试源文件
file(GLOB SELF_MADE_CPP_TEST_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/test/self_made_cpp/*.cpp")
message(STATUS "SELF_MADE_CPP_TEST_SOURCES: ${SELF_MADE_CPP_TEST_SOURCES}")

# 创建测试可执行文件
add_executable(self_made_cpp_test ${SELF_MADE_CPP_TEST_SOURCES})

target_link_libraries(self_made_cpp_test gtest gtest_main self_made_cpp)

# 添加测试
enable_testing()
include(GoogleTest)
gtest_discover_tests(self_made_cpp_test)

# 设置测试输出目录
set(CTEST_BINARY_DIRECTORY ${CMAKE_BINARY_DIR}/custom_testing_dir)
message(STATUS "self_made_cpp_test_BINARY_DIRECTORY: ${CTEST_BINARY_DIRECTORY}")

# 设置可执行文件的输出目录
set_target_properties(self_made_cpp_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)