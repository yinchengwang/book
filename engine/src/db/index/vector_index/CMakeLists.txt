# 向量索引模块 CMakeLists.txt

# 收集源文件（排除有编译问题的子目录）
file(GLOB_RECURSE VECTOR_INDEX_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

# 排除有头文件依赖问题的目录
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*streaming/.*")
# hybrid_search.c 此前因枚举/结构体改名与 query_filter.h 不同步而被排除，
# 导致 filter_expr_* / hybrid_index_* / hybrid_search_config_* 共 27 个符号
# 无任何库提供，链接方（test_vector_index）报 undefined reference。
# 已将源码对齐 VDB_FILTER_OP_* / VDBFilterValue 布局，此处解除排除。
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*tiered_index\\.c$")
# DiskANN 通过独立子目录构建（diskann/CMakeLists.txt），避免被主 glob 重复收集
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*diskann/.*")
# faiss_hnsw 同理：独立静态库 faiss_hnsw 由 faiss_hnsw/CMakeLists.txt 构建，
# 主 glob 收集其源码会因头文件/声明不同步而编译失败
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*faiss_hnsw/.*")
# GPU 加速为 CUDA/OpenCL 存根，私有头（include/db/index/vector_index/gpu/）
# 不在本目标包含路径上，且无外部符号引用；待其独立成库后再接入
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX ".*gpu/.*")
# HNSW-PQ 同理：独立静态库 hnsw_pq 由 hnsw_pq/CMakeLists.txt 构建。
# 主 glob 若再收集其源码，同一份 hnsw_pq.c 会进两个库，符号重复导出。
# 更严重的是 hnsw_pq.c 调用 pq_train/pq_encode/pq_code_size 等量化器接口，
# 其签名是 pq_quantizer_t*（db 侧量化器）；若本库内解析到 algo-prod 的
# 同名符号（quantizer_t*），C 不做重载检查 → 链接通过但运行期类型错配。
# 故此处排除，由独立库 hnsw_pq 独家提供（它 PUBLIC 链接 db 侧 pq 库）。
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX "^hnsw_pq/.*")
# 量化器子目录（pq/sq/rq/lvq）各自已建独立静态库（见各自的 CMakeLists.txt），
# 主 glob 若再收集其源码，会与 algo-prod 中的同名实现产生重复符号：
# pq_code_size / pq_encode / pq_train / pq_create / pq_destroy /
# sq_encode / sq_train / rq_encode / rq_train / lvq_encode 等 17 个符号
# 在链接期报 multiple definition。此处排除，与 diskann/faiss_hnsw 的处理一致。
# 注意：正则需锚定目录名，否则 ".*pq/.*" 会误伤 hnsw_pq/、ivf_pq/、opq/ 等。
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX "^pq/.*")
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX "^sq/.*")
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX "^rq/.*")
list(FILTER VECTOR_INDEX_SOURCES EXCLUDE REGEX "^lvq/.*")

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
