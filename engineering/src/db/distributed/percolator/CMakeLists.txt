# 分布式 Percolator 事务模块
add_library(db_percolator STATIC
    percolator.c
    percolator_recover.c
)

target_include_directories(db_percolator PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/distributed
)

target_link_libraries(db_percolator PUBLIC
    db_core
    db_mvcc_ts
)

target_compile_options(db_percolator PRIVATE -Wno-unused-parameter)

set_target_properties(db_percolator PROPERTIES FOLDER "Libraries")