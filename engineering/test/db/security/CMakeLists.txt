# 安全管理系统单元测试

add_executable(security_test security_test.cpp)
target_link_libraries(security_test
    PRIVATE
        db_security
        db_core
        project_includes
        gtest
        gtest_main
)
target_include_directories(security_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/security
)
add_test(NAME security_test COMMAND security_test)