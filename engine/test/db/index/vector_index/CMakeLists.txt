# ReRanker 测试 - 独立构建

file(GLOB_RECURSE VECTOR_RERANKER_TEST_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

# 只包含 reranker 测试
set(RERANKER_ONLY_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_reranker.cpp
)

add_executable(vector_reranker_test ${RERANKER_ONLY_TEST_SOURCES})

target_include_directories(vector_reranker_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${ENGINEERING_SOURCE_DIR}/src/db/index/vector_index
)

# 手动链接需要的库
target_link_libraries(vector_reranker_test PRIVATE
    vector_reranker
    algo-prod
    gtest
    gtest_main
    Threads::Threads
)

# gtest_discover_tests 让 ctest 能发现 reranker 测试
include(GoogleTest)
gtest_discover_tests(vector_reranker_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "vector_index"
)

# 向量删除模块测试
add_subdirectory(delete)

# NSW 索引测试
add_subdirectory(nsw)

# SSG 索引测试
add_subdirectory(ssg)

# PQ 量化器测试
add_subdirectory(pq)

# SQ 量化器测试
add_subdirectory(sq)

# HNSW-PQ 混合索引测试
add_subdirectory(hnsw_pq)

# IVF-PQ 混合索引测试
add_subdirectory(ivf_pq)

# IVF-Flat 索引测试
add_subdirectory(ivf_flat)

# Annoy 索引测试
add_subdirectory(annoy)

# KD-Tree 索引测试
add_subdirectory(kd_tree)

# Multi-Probe LSH 索引测试
add_subdirectory(lsh_multiprobe)

# ScaNN 索引测试
add_subdirectory(scann)

# RQ 量化器测试
add_subdirectory(rq)

# LVQ 量化器测试
add_subdirectory(lvq)

# Ball-Tree 索引测试
add_subdirectory(ball_tree)

# Spectral Hash 测试
add_subdirectory(spectral_hash)

# HNSW-SQ 混合索引测试
add_subdirectory(hnsw_sq)

# IVF-HNSW 混合索引测试
add_subdirectory(ivf_hnsw)

# ITQ 量化器测试
add_subdirectory(itq)

# Milvus IVF 风格索引测试
add_subdirectory(milvus_ivf)

# Faiss IVF 兼容层测试
add_subdirectory(faiss_ivf_compat)
