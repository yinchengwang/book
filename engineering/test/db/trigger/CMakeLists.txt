# 数据库触发器测试

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_test(db_trigger_test DB_TRIGGER_TESTS
    db_storage
)
