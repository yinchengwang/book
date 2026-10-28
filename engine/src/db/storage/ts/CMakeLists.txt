# storage/ts 模块：时序存储引擎
file(GLOB_RECURSE TS_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(TS_SOURCE_FILES)
    # Task 13：独立静态库（头文件字段缺失/CdcConsumer 双定义已修）
    add_library(mm_ts STATIC ${TS_SOURCE_FILES})
    target_include_directories(mm_ts PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include/db/storage/ts
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
        ${ENGINEERING_SOURCE_DIR}/third_part/uthash/src
    )
    # L1 设施（nm 实测：wal/lock/mm_pool）
    target_link_libraries(mm_ts PUBLIC mm_base storage_wal storage_lock storage_mm storage_page)
    set_target_properties(mm_ts PROPERTIES FOLDER "ModalityEngines")
endif()
