file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_project_test(${TEST_NAME} TEST_FILES db_bgworker)
    # 添加任务接口头文件目录到包含路径（bgworker/tasks/task_iface.h）
    target_include_directories(${TEST_NAME} PRIVATE
        ${ENGINEERING_SOURCE_DIR}/src/db/bgworker
    )
endforeach()
