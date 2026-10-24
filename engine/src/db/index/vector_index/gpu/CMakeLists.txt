# GPU 向量索引模块
# 提供 GPU 加速的向量索引实现

# 源文件列表
set(GPU_INDEX_SOURCES
    gpu_device.c
    gpu_memory.c
    gpu_hnsw.c
    gpu_ivf.c
    gpu_ivf_pq.c
    simd.c
    PARENT_SCOPE)  # 导出到父目录
