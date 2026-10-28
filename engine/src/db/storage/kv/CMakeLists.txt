# storage/kv 模块：KV 存储引擎
# Task 7：从 OBJECT（编入 db_storage 聚合）改为独立静态库 mm_kv，
# 作为「每模态一个独立静态库」的模板。kv profile 下冒烟目标
# mm_kv_smoke 只链 mm_base + mm_storage_api + mm_kv 即可链接，
# 链不上任何其他模态正是隔离的机械证明。
file(GLOB_RECURSE KV_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(KV_SOURCE_FILES)
    add_library(mm_kv STATIC ${KV_SOURCE_FILES})
    target_include_directories(mm_kv PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
    # L1 存储设施：kv 引擎依赖 buffer/disk/wal（对象库嵌入本 archive，
    # 与 mm_vector->storage_page 同模式；db_storage 聚合内的同名副本
    # 由链接器 pull-once 规则去重）
    target_link_libraries(mm_kv PUBLIC mm_base storage_buffer storage_page storage_wal)
    set_target_properties(mm_kv PROPERTIES FOLDER "ModalityEngines")
endif()
