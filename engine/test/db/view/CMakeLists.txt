# 数据库视图测试

include(${ENGINEERING_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_test(db_view_test DB_VIEW_TESTS
    db_storage
)
