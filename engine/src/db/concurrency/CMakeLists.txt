# MVCC 并发控制库
file(GLOB_RECURSE MVCC_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(MVCC_SOURCE_FILES)
    add_library(db_mvcc STATIC ${MVCC_SOURCE_FILES})
    target_include_directories(db_mvcc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/concurrency
    )
    target_link_libraries(db_mvcc PRIVATE project_includes)
endif()