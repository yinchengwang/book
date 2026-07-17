# COPY 工具测试
add_executable(test_copy test_copy.cpp)
target_link_libraries(test_copy
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_copy
)
set_target_properties(test_copy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
include(GoogleTest)
gtest_discover_tests(test_copy
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_copy"
)

# CSV 格式 COPY 测试
add_executable(test_copy_csv test_copy_csv.cpp)
target_link_libraries(test_copy_csv
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_copy
)
set_target_properties(test_copy_csv PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_copy_csv
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_copy_csv"
)

# TEXT 格式 COPY 测试
add_executable(test_copy_text test_copy_text.cpp)
target_link_libraries(test_copy_text
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_copy
)
set_target_properties(test_copy_text PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_copy_text
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_copy_text"
)

# JSON 格式 COPY 测试
add_executable(test_copy_json test_copy_json.cpp)
target_link_libraries(test_copy_json
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_copy
)
set_target_properties(test_copy_json PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_copy_json
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_copy_json"
)

# SQL 格式 COPY 测试
add_executable(test_copy_sql test_copy_sql.cpp)
target_link_libraries(test_copy_sql
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_copy
)
set_target_properties(test_copy_sql PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_copy_sql
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_copy_sql"
)

# 二进制格式 COPY 测试
add_executable(test_copy_binary test_copy_binary.cpp)
target_link_libraries(test_copy_binary
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_copy
)
set_target_properties(test_copy_binary PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_copy_binary
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_copy_binary"
)

# 系统表管理工具测试
add_executable(test_sys_catalog test_sys_catalog.cpp)
target_link_libraries(test_sys_catalog
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_sys_catalog
)
set_target_properties(test_sys_catalog PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_sys_catalog
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_sys_catalog"
)

# 统计收集器测试
add_executable(test_stats test_stats.cpp)
target_link_libraries(test_stats
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_stats
)
set_target_properties(test_stats PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_stats
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_stats"
)

# 统计视图测试
add_executable(test_stat_views test_stat_views.cpp)
target_link_libraries(test_stat_views
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_stats
)
set_target_properties(test_stat_views PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_stat_views
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_stat_views"
)

# EXPLAIN 执行计划分析测试
add_executable(test_explain test_explain.cpp)
target_link_libraries(test_explain
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_explain
)
set_target_properties(test_explain PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_explain
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_explain"
)

# EXPLAIN YAML 格式测试
add_executable(test_explain_yaml test_explain_yaml.cpp)
target_link_libraries(test_explain_yaml
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_explain
)
set_target_properties(test_explain_yaml PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_explain_yaml
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_explain_yaml"
)

# VACUUM 空间回收测试
add_executable(test_vacuum test_vacuum.cpp)
target_link_libraries(test_vacuum
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_vacuum
)
set_target_properties(test_vacuum PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_vacuum
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_vacuum"
)

# REINDEX 索引重建测试
add_executable(test_reindex test_reindex.cpp)
target_link_libraries(test_reindex
    PRIVATE
        project_includes
        GTest::gtest_main
        db_tools_reindex
)
set_target_properties(test_reindex PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    FOLDER "Tests"
)
gtest_discover_tests(test_reindex
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "test_reindex"
)
