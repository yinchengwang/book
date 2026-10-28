# storage/yang 模块：树存储引擎
file(GLOB_RECURSE YANG_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(YANG_SOURCE_FILES)
    add_library(mm_yang STATIC ${YANG_SOURCE_FILES})
    target_include_directories(mm_yang PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
    # Task 10：nm 实测仅 wal（wal_get_current/wal_write_yang_ds）+ L0
    target_link_libraries(mm_yang PUBLIC mm_base storage_wal storage_page)
    set_target_properties(mm_yang PROPERTIES FOLDER "ModalityEngines")
endif()
