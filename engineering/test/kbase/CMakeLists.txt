# kbase 测试

# 张量系统测试
add_project_test(test_infra_tensor KBASE_TENSOR_TESTS
    kbase
    db_storage
)

# 算子测试
add_project_test(test_infra_ops KBASE_OPS_TESTS
    kbase
)

# 模型加载测试
add_project_test(test_infra_model KBASE_MODEL_TESTS
    kbase
)

# Embedding 测试
add_project_test(test_infra_embed KBASE_EMBED_TESTS
    kbase
)

# 量化测试（Phase 2）
# add_project_test(test_infra_quant KBASE_QUANT_TESTS kbase)

# SIMD 测试（Phase 2）
# add_project_test(test_infra_simd KBASE_SIMD_TESTS kbase)

# kbase 索引测试
add_project_test(test_kbase_index KBASE_INDEX_TESTS
    kbase
    vector_index
)

# kbase 搜索测试
add_project_test(test_kbase_search KBASE_SEARCH_TESTS
    kbase
    vector_index
)

# kbase 端到端测试
add_project_test(test_kbase_e2e KBASE_E2E_TESTS
    kbase
    vector_index
)
