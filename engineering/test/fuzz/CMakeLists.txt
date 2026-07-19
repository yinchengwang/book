# ============================================================
# Fuzz Testing - 模糊测试
# ============================================================

# === Page fuzzer (active) ===
add_project_test(test_fuzz_page VAR)
target_link_libraries(test_fuzz_page PRIVATE db_storage db_core)