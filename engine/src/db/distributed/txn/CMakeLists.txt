# 分布式事务模块
add_library(db_distributed_txn STATIC
    distributed_txn.c
)

target_include_directories(db_distributed_txn PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/distributed/txn
)

target_link_libraries(db_distributed_txn PUBLIC
    db_core
)

target_compile_options(db_distributed_txn PRIVATE -Wno-unused-parameter)

set_target_properties(db_distributed_txn PROPERTIES FOLDER "Libraries")
