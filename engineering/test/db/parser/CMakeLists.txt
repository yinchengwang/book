# 数据库解析器测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

# SQL 解析器测试
# T4: archived with stack A, rewritten in T12
# （该测试链接 db_parser_sql_simplified 并使用 makefuncs/parse_node 符号；
#   简化解析器库已移除，helper 源文件保留至 T6 编入 canonical 目标后再恢复）
# add_project_test(db_parser_sql_test DB_PARSER_SQL_TESTS
#     db_parser_sql_simplified
#     project_includes
# )
