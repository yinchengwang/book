# 视图模块
file(GLOB_RECURSE VIEW_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(VIEW_SOURCE_FILES)
    add_library(db_view STATIC ${VIEW_SOURCE_FILES})
    target_include_directories(db_view PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/view
    )
    target_link_libraries(db_view PRIVATE project_includes)
endif()