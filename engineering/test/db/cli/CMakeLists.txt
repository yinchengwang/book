# CLI 模块测试 - 暂时禁用（依赖已禁用的 db_cli_lib）
# add_executable(test_cli cli_test.cpp)
# target_link_libraries(test_cli db_cli_lib db_parser db_executor db_storage db_utils project_includes gtest gtest_main)

# add_custom_target(test_cli_test
#     COMMAND test_cli
#     DEPENDS test_cli
# )