# ============================================================
# Fuzz Testing - 模糊测试
# ============================================================

# === Page fuzzer (active) ===
add_project_test(test_fuzz_page VAR)
target_link_libraries(test_fuzz_page PRIVATE db_storage db_core)

# === SQL fuzzer (enabled - uses sql_parse_one API) ===
add_executable(test_fuzz_sql sql_fuzzer.cpp)
target_link_libraries(test_fuzz_sql PRIVATE db_parser_sql_simplified project_includes gtest gtest_main)

# === WAL fuzzer (enabled - uses wal_redo API) ===
add_executable(test_fuzz_wal wal_fuzzer.cpp)
target_link_libraries(test_fuzz_wal PRIVATE db_storage db_core project_includes gtest gtest_main)