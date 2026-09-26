# CLI 模块测试（T10 重接线）
#
# 重新启用 test_cli_smoke：依赖 db_cli_lib（canonical SQL 路径，无 vector_api）。
# 注意：db_cli_lib 已在 src/db/cli/CMakeLists.txt 中定义并链接 sql_engine +
# db_storage + db_parser_sql + db_core。

add_executable(test_cli_smoke test_cli_smoke.cpp)
target_include_directories(test_cli_smoke PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(test_cli_smoke
    db_cli_lib
    project_includes
    gtest
    gtest_main
)
set_target_properties(test_cli_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
include(GoogleTest)
gtest_discover_tests(test_cli_smoke
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "CliSmoke"
)
