# 添加测试目标的辅助函数
#
# add_project_test(<name> <sources_var> [SOURCES <file>...])
#
# 默认行为：GLOB 当前目录下所有 .cpp/.c 编入该目标。
# 该默认行为在「一个目录多个源文件、多个目标」时会重复编译同一源文件，
# 造成同一 gtest 用例在 ctest 中注册多份；这些副本若共用固定数据文件
# （如 test/sdk/memory 的 *.db），并行执行时互相踩踏，表现为随机失败。
# 这种目录应改用 SOURCES 显式指定各自拥有的源文件。
#
# SOURCES 必须是最后一个关键字，其后所有参数都视为源文件；
# 需要额外链接库时写在 SOURCES 之前。
function(add_project_test TEST_NAME TEST_SOURCES_VAR)
    # 分离 SOURCES 关键字与链接库参数
    set(_libs "")
    set(_explicit "")
    set(_in_sources OFF)
    foreach(_arg IN LISTS ARGN)
        if(_arg STREQUAL "SOURCES")
            set(_in_sources ON)
        elseif(_in_sources)
            list(APPEND _explicit "${_arg}")
        else()
            list(APPEND _libs "${_arg}")
        endif()
    endforeach()

    # 获取测试源文件
    if(_explicit)
        set(${TEST_SOURCES_VAR} ${_explicit})
    else()
        file(GLOB ${TEST_SOURCES_VAR}
            CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        )
    endif()

    # 如果有测试源文件，创建测试可执行文件
    if(${TEST_SOURCES_VAR})
        add_executable(${TEST_NAME} ${${TEST_SOURCES_VAR}})

        target_link_libraries(${TEST_NAME}
            PRIVATE
                project_includes
                GTest::gtest_main
                ${_libs}  # 额外的库参数
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