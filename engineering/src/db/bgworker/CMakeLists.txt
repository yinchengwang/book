# DB 后台任务调度器

# 仅收集 bgworker 顶层目录的源文件（非递归），
# tasks 子目录的源文件由其自身 CMakeLists.txt 通过 target_sources 追加，
# 从而保证 add_subdirectory(tasks) 在 db_bgworker 目标创建之后执行
file(GLOB BGWORKER_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(BGWORKER_SOURCE_FILES)
    add_library(db_bgworker STATIC ${BGWORKER_SOURCE_FILES})
    target_include_directories(db_bgworker PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/tasks
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
    )
    target_link_libraries(db_bgworker PRIVATE
        db_core
        db_cceh
        Threads::Threads
    )

    # 目标创建后再处理 tasks 子目录，确保 target_sources 能找到 db_bgworker
    add_subdirectory(tasks)
endif()
