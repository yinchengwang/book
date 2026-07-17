# 数据库解析器测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

# SQL 解析器测试
add_project_test(db_parser_sql_test DB_PARSER_SQL_TESTS
    db_parser_sql_simplified
    project_includes
)
