# protocol 测试
# MySQL/PostgreSQL 线协议单元测试

set(PROTOCOL_TEST_SOURCES
    mysql_wire_test.cpp
    pg_wire_test.cpp
)

# 创建测试可执行文件
add_executable(test_protocol ${PROTOCOL_TEST_SOURCES})

target_link_libraries(test_protocol
    protocol
    db_core
    project_includes
    gtest
    gtest_main
)

target_include_directories(test_protocol PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)

# 添加到全量测试目标（使用 CMake 3.18+ 的 AFTER 关键字延迟依赖）
if(TARGET db_all_tests)
    add_dependencies(db_all_tests test_protocol)
endif()

# 设置输出目录
set_target_properties(test_protocol PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
