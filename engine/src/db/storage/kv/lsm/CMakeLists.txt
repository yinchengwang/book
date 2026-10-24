# LSM-Tree 存储引擎模块
add_library(db_kv_lsm STATIC
    lsm_tree.c
    sstable.c
)

target_include_directories(db_kv_lsm PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/storage/kv/lsm
)

target_link_libraries(db_kv_lsm PUBLIC
    db_core
)

target_compile_options(db_kv_lsm PRIVATE -Wno-unused-parameter -Wno-missing-field-initializers)

set_target_properties(db_kv_lsm PROPERTIES FOLDER "Libraries")
