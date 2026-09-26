# 数据库解析器测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

# SQL 解析器测试
# T4: archived with stack A, rewritten in T12
# （test_parser_sql.cpp 链接已移除的 db_parser_sql_simplified 且自带 main()，
#   仍保持注销状态；add_project_test 的 glob 会把它一并收编，故 T6 测试用显式源列表）
# add_project_test(db_parser_sql_test DB_PARSER_SQL_TESTS
#     db_parser_sql_simplified
#     project_includes
# )

# T6: canonical parser 统一入口测试
add_executable(test_sql_parse_canonical test_sql_parse_canonical.cpp)
target_link_libraries(test_sql_parse_canonical
    PRIVATE
        project_includes
        GTest::gtest_main
        db_parser_sql
)
set_target_properties(test_sql_parse_canonical PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
include(GoogleTest)
gtest_discover_tests(test_sql_parse_canonical
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_sql_parse_canonical"
)
