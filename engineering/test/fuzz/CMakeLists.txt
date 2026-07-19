# ============================================================
# Fuzz Testing - 模糊测试
# ============================================================

# === Page fuzzer (active) ===
add_project_test(test_fuzz_page VAR)
target_link_libraries(test_fuzz_page PRIVATE db_storage db_core)

# === SQL fuzzer (disabled - sql_parser.c has compatibility issues on Windows) ===
# sql_parser.c 包含 sql_parse_one 实现，但被 sql/CMakeLists.txt 排除了，因此 fuzzer 无法链接
# add_executable(test_fuzz_sql sql_fuzzer.cpp)
# target_link_libraries(test_fuzz_sql PRIVATE db_parser_sql_simplified project_includes gtest gtest_main)

# === WAL fuzzer (enabled - uses wal_redo API) ===
add_executable(test_fuzz_wal wal_fuzzer.cpp)
target_link_libraries(test_fuzz_wal PRIVATE db_storage db_core project_includes gtest gtest_main)