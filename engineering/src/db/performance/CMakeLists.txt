# 性能优化模块
file(GLOB_RECURSE PERF_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(PERF_SOURCE_FILES)
    add_library(db_performance STATIC ${PERF_SOURCE_FILES})
    target_include_directories(db_performance PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/performance
    )
    target_link_libraries(db_performance PRIVATE project_includes)
endif()