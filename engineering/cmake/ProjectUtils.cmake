# 添加测试目标的辅助函数
function(add_project_test TEST_NAME TEST_SOURCES_VAR)
    # 获取测试源文件
    file(GLOB ${TEST_SOURCES_VAR}
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    )

    # 如果有测试源文件，创建测试可执行文件
    if(${TEST_SOURCES_VAR})
        add_executable(${TEST_NAME} ${${TEST_SOURCES_VAR}})

        target_link_libraries(${TEST_NAME}
            PRIVATE
                project_includes
                GTest::gtest_main
                ${ARGN}  # 额外的库参数
        )

        # 设置测试属性（编译产物统一进入 build/ 目录，不再写入源码目录）
        set_target_properties(${TEST_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
            FOLDER "Tests"
        )

        # 添加到CTest
        include(GoogleTest)
        gtest_discover_tests(${TEST_NAME}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            PROPERTIES LABELS "${TEST_NAME}"
        )

        message(STATUS "Added ${TEST_NAME} tests: ${${TEST_SOURCES_VAR}}")
    else()
        message(WARNING "No test sources found for ${TEST_NAME} in ${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endfunction()

# 添加库目标的辅助函数
function(add_project_library LIB_NAME LIB_SOURCES_VAR)
    # 获取库源文件
    file(GLOB ${LIB_SOURCES_VAR}
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    )

    if(${LIB_SOURCES_VAR})
        add_library(${LIB_NAME} ${${LIB_SOURCES_VAR}})

        target_link_libraries(${LIB_NAME}
            PRIVATE
                project_includes
        )

        set_target_properties(${LIB_NAME} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
            FOLDER "Libraries"
        )

        message(STATUS "Added ${LIB_NAME} library: ${${LIB_SOURCES_VAR}}")
    else()
        message(WARNING "No sources found for ${LIB_NAME} in ${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endfunction()