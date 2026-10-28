file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

# 每个目标只编自己的源文件（SOURCES 显式指定）。
# add_project_test 默认 GLOB 当前目录所有 .cpp，会把每个源文件编进
# 循环里的每个目标，使同一 gtest 用例在 ctest 中注册多份。
foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_project_test(${TEST_NAME} TEST_FILES db_bgworker
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_FILE})
    # 添加任务接口头文件目录到包含路径（bgworker/tasks/task_iface.h）
    target_include_directories(${TEST_NAME} PRIVATE
        ${ENGINEERING_SOURCE_DIR}/src/db/bgworker
    )
endforeach()
