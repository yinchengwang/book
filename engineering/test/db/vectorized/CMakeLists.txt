# Gap#2 向量化执行引擎测试
enable_testing()

add_executable(vectorized_exec_test
    block_test.cpp
)
target_link_libraries(vectorized_exec_test
    db_vectorized
    db_core
    project_includes
    gtest
    gtest_main
)
target_include_directories(vectorized_exec_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/vectorized
)

add_test(NAME vectorized_exec_test COMMAND vectorized_exec_test)
