# 协议层测试
if(BUILD_TESTING)
    add_executable(test_resp
        ${ENGINEERING_SOURCE_DIR}/test/db/protocol/resp_test.cpp
    )
    target_include_directories(test_resp PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/protocol
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_resp PRIVATE
        db_protocol
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME resp_test COMMAND test_resp)
endif()