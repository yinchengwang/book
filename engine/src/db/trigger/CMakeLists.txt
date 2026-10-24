# 触发器模块
file(GLOB_RECURSE TRIGGER_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(TRIGGER_SOURCE_FILES)
    add_library(db_trigger STATIC ${TRIGGER_SOURCE_FILES})
    target_include_directories(db_trigger PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/trigger
    )
    target_link_libraries(db_trigger PRIVATE project_includes)
endif()