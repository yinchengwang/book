# 主从复制模块
file(GLOB_RECURSE REPL_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(REPL_SOURCE_FILES)
    add_library(db_replication STATIC ${REPL_SOURCE_FILES})
    target_include_directories(db_replication PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/replication
        ${ENGINEERING_SOURCE_DIR}/include/sdk
        ${ENGINEERING_SOURCE_DIR}/include/sdk/impl
    )
    # Task 12：复制模块迁移到 MemoryContext，依赖 SDK 兼容层
    target_link_libraries(db_replication PRIVATE project_includes mmsdk)
endif()