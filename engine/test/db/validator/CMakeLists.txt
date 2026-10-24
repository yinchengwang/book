# validator 模块测试（SQL 语义分析，T5 起走 catalog API）
add_executable(test_sql_semantic test_sql_semantic.cpp)

target_link_libraries(test_sql_semantic
    PRIVATE
    db_validator
    db_storage
    db_core
    project_includes
    gtest
    gtest_main
)

add_test(NAME test_sql_semantic COMMAND test_sql_semantic)
