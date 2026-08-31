# 分布式事务增量测试
enable_testing()

add_executable(distributed_txn_test
    tso_test.cpp
    mvcc_ts_test.cpp
    percolator_test.cpp
    deadlock_test.cpp
)
target_link_libraries(distributed_txn_test
    db_tso
    db_mvcc_ts
    db_percolator
    db_deadlock
    db_core
    project_includes
    gtest
    gtest_main
)
target_include_directories(distributed_txn_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/distributed
)

add_test(NAME distributed_txn_test COMMAND distributed_txn_test)