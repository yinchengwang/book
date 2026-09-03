# apps 模块测试

# 测试公共组件
add_executable(test_common_terminal_test
    test_common_terminal.cpp
)

target_link_libraries(test_common_terminal_test
    PRIVATE
        project_includes
        apps_common
        GTest::gtest_main
)

# 菜单测试（依赖终端测试）
add_executable(test_common_menu_test
    test_common_menu.cpp
)

target_link_libraries(test_common_menu_test
    PRIVATE
        project_includes
        apps_common
        GTest::gtest_main
)

# Todo App 模型测试（暂时注释掉，源文件不存在）
# add_executable(test_todo_model_test
#     test_todo_simple.cpp
#     ${CMAKE_SOURCE_DIR}/apps/todo-app/todo_model.c
# )
#
# target_include_directories(test_todo_model_test PRIVATE
#     ${CMAKE_SOURCE_DIR}/apps/todo-app
#     ${CMAKE_SOURCE_DIR}/third_part
# )
#
# target_link_libraries(test_todo_model_test
#     PRIVATE
#         project_includes
#         cjson
#         GTest::gtest_main
# )

# 添加测试
include(GoogleTest)
gtest_discover_tests(test_common_terminal_test)
gtest_discover_tests(test_common_menu_test)
# gtest_discover_tests(test_todo_model_test)

# 全量测试目标
add_custom_target(apps_all_tests
    COMMAND echo "Running all apps tests..."
    DEPENDS
        test_common_terminal_test
        test_common_menu_test
        # test_todo_model_test
)
