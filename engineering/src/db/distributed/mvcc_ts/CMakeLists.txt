# 分布式 MVCC 时间戳模块
add_library(db_mvcc_ts STATIC
    mvcc_ts.c
)

target_include_directories(db_mvcc_ts PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/distributed
)

target_link_libraries(db_mvcc_ts PUBLIC
    db_core
)

target_compile_options(db_mvcc_ts PRIVATE -Wno-unused-parameter)

set_target_properties(db_mvcc_ts PROPERTIES FOLDER "Libraries")