# 向量索引模块 CMakeLists.txt

# 收集源文件（排除有编译问题的子目录）
file(GLOB_RECURSE VECTOR_INDEX_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

# 排除有头文件依赖问题的目录
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*streaming/.*")
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*hybrid_search\\.c$")
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*tiered_index\\.c$")
# DiskANN 通过独立子目录构建（diskann/CMakeLists.txt），避免被主 glob 重复收集
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*diskann/.*")
# faiss_hnsw 同理：独立静态库 faiss_hnsw 由 faiss_hnsw/CMakeLists.txt 构建，
# 主 glob 收集其源码会因头文件/声明不同步而编译失败
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*faiss_hnsw/.*")
# GPU 加速为 CUDA/OpenCL 存根，私有头（include/db/index/vector_index/gpu/）
# 不在本目标包含路径上，且无外部符号引用；待其独立成库后再接入
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*gpu/.*")

add_library(vector_index STATIC ${VECTOR_INDEX_SOURCES})

target_include_directories(vector_index PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(vector_index PRIVATE project_includes algo-prod Threads::Threads)
target_link_libraries(vector_index PUBLIC faiss_hnsw)

# 持久化控制模块（被向量索引使用以管理 persist_enabled 开关）
target_sources(vector_index PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../index_persist_control.c
)
target_include_directories(vector_index PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include/db/index
)

# DiskANN 子目录（独立构建独立库）
add_subdirectory(diskann)

# ReRanker 精排模块
add_subdirectory(reranker)

# BM25 文本检索模块
add_subdirectory(BM25)

# IVF 模块
add_subdirectory(ivf)

# NSW 模块
add_subdirectory(nsw)

# SSG 模块
add_subdirectory(ssg)

# PQ 量化器
add_subdirectory(pq)

# SQ 量化器
add_subdirectory(sq)

# HNSW-PQ 混合索引
add_subdirectory(hnsw_pq)

# IVF-Flat 索引
add_subdirectory(ivf_flat)

# Annoy 索引
add_subdirectory(annoy)

# KD-Tree 索引
add_subdirectory(kd_tree)

# Multi-Probe LSH 索引
add_subdirectory(lsh_multiprobe)

# ScaNN 索引
add_subdirectory(scann)

# RQ 残差量化器
add_subdirectory(rq)

# LVQ 学习向量量化器
add_subdirectory(lvq)

# Ball-Tree 索引
add_subdirectory(ball_tree)

# Spectral Hash 频谱哈希
add_subdirectory(spectral_hash)

# HNSW-SQ 混合索引
add_subdirectory(hnsw_sq)

# IVF-HNSW 混合索引
add_subdirectory(ivf_hnsw)

# ITQ 迭代量化
add_subdirectory(itq)

# Milvus IVF 风格索引
add_subdirectory(milvus_ivf)

# Faiss IVF 兼容层
add_subdirectory(faiss_ivf_compat)

# Faiss HNSW 索引（参考 FAISS HNSW.cpp 重写的内存版）
add_subdirectory(faiss_hnsw)

# GPU 向量加速模块（CUDA/OpenCL 加速）
add_subdirectory(gpu)

# 分布式 HNSW 索引（待 faiss_hnsw 问题修复后启用）
# add_subdirectory(dist_hnsw)
