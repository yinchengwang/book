# 查询优化器库
file(GLOB_RECURSE OPTIMIZER_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(OPTIMIZER_SOURCE_FILES)
    add_library(db_optimizer STATIC ${OPTIMIZER_SOURCE_FILES})
    target_include_directories(db_optimizer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/optimizer
    )
    target_link_libraries(db_optimizer PRIVATE project_includes)
endif()