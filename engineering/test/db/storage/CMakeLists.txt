# 测试 sparse/BM25/hybrid
# engineering/test/db/storage/CMakeLists.txt

# 编译 test_sparse.c 为可执行文件
add_executable(test_sparse test_sparse.c)

target_link_libraries(test_sparse
    PRIVATE
    storage_sparse
    db_storage
    db_core
)

# 自动发现 test_sparse 中的 main 用例
add_test(NAME test_sparse COMMAND test_sparse)
