# Constraints 模块测试

add_executable(test_constraints test_constraints.cpp)
target_link_libraries(test_constraints db_constraints project_includes gtest gtest_main)

add_custom_target(test_constraints_test
    COMMAND test_constraints
    DEPENDS test_constraints
)