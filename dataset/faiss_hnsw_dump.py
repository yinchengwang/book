"""
faiss_hnsw_dump.py

在 FAISS 中用相同参数（M=16, efConstruction=200）构建 HNSW 索引，
导出图结构信息，与我们的 C 实现对比。
"""

import numpy as np
import struct
import sys
import os
import faiss

def read_fvecs(path):
    """读取 fvecs 格式文件"""
    with open(path, 'rb') as f:
        data = f.read()

    vectors = []
    pos = 0
    while pos < len(data):
        dim = struct.unpack('i', data[pos:pos+4])[0]
        pos += 4
        vec = struct.unpack('f' * dim, data[pos:pos+dim*4])
        pos += dim * 4
        vectors.append(list(vec))
    return np.array(vectors, dtype=np.float32)

# 读取 SIFT Small 数据
base_path = "c:/code/book/dataset/siftsmall_processed/base.bin"
base = read_fvecs(base_path)
print(f"数据集: {base.shape[0]} 向量, {base.shape[1]} 维度")

M = 16
efConstruction = 200

index = faiss.IndexHNSWFlat(base.shape[1], M, faiss.METRIC_L2)
index.hnsw.efConstruction = efConstruction
index.hnsw.set_default_probas(M, 1.0 / np.log(M))

print(f"\n构建 HNSW 索引 (M={M}, efConstruction={efConstruction})...")
index.add(base)
print(f"索引构建完成")

hnsw = index.hnsw

# 用 vector_to_array 将 FAISS 内部分配的向量转为 numpy 数组
levels_arr = faiss.vector_to_array(hnsw.levels)
neighbors_arr = faiss.vector_to_array(hnsw.neighbors)
offsets_arr = faiss.vector_to_array(hnsw.offsets)
cum_arr = faiss.vector_to_array(hnsw.cum_nneighbor_per_level)

n_total = len(levels_arr)
print(f"\n  DEBUG: levels_arr.shape={levels_arr.shape}, neighbors_arr.shape={neighbors_arr.shape}")
print(f"  DEBUG: offsets_arr.shape={offsets_arr.shape}, cum_arr.shape={cum_arr.shape}")
print(f"  DEBUG: cum_arr={cum_arr.tolist()}")

# ================================================================
# 1. 基本信息
# ================================================================
print("\n========================================")
print("FAISS HNSW 图结构诊断报告")
print("========================================")
print(f"\n--- 1. 基本信息 ---")
print(f"  n_total: {n_total}")
print(f"  entry_point: {hnsw.entry_point}")
print(f"  max_level: {hnsw.max_level}")
print(f"  M: {M}")
print(f"  ef_construction: {efConstruction}")
print(f"  neighbors 数组大小: {len(neighbors_arr)}")

# ================================================================
# 2. 层级分布
# ================================================================
print(f"\n--- 2. 层级分布 ---")
max_lvl = int(levels_arr.max())
hist = [0] * (max_lvl + 1)
for lvl in levels_arr:
    hist[lvl] += 1

print(f"  层级 | 节点数 | 占比")
print(f"  ------|--------|------")
for l in range(max_lvl + 1):
    print(f"  {l:4d}  | {hist[l]:6d} | {100.0*hist[l]/n_total:.2f}%")

# ================================================================
# 3. 每层邻居容量
# ================================================================
print(f"\n--- 3. 每层邻居容量 ---")
for l in range(len(cum_arr)):
    if l == 0:
        cap = cum_arr[l]
    else:
        cap = cum_arr[l] - cum_arr[l-1]
    print(f"  layer {l}: {cap} 个邻居槽位")

# ================================================================
# 4. 前 20 个节点的邻接表
# ================================================================
print(f"\n--- 4. 前 20 个节点的邻接表 ---")
n_dump = min(20, n_total)

def neighbor_range(no, layer_no):
    o = offsets_arr[no]
    begin = int(o + cum_arr[layer_no])
    end = int(o + cum_arr[layer_no + 1])
    return begin, end

for i in range(n_dump):
    pt_level = int(levels_arr[i])
    print(f"  Node {i:4d} (level={pt_level}):")
    for l in range(pt_level + 1):
        begin, end = neighbor_range(i, l)
        neighbors = []
        for j in range(begin, end):
            nid = neighbors_arr[j]
            if nid >= 0:
                neighbors.append(nid)
        print(f"    layer {l}: {' '.join(str(n) for n in neighbors)} ({len(neighbors)} 个)")

# ================================================================
# 5. 邻居统计
# ================================================================
print(f"\n--- 5. 邻居统计 (仅 level 0) ---")
total_nbrs = 0
max_nbrs = 0
min_nbrs = 999999
nbr_hist = {}

for i in range(n_total):
    begin, end = neighbor_range(i, 0)
    count = int(np.sum(neighbors_arr[begin:end] >= 0))
    total_nbrs += count
    max_nbrs = max(max_nbrs, count)
    min_nbrs = min(min_nbrs, count)
    nbr_hist[count] = nbr_hist.get(count, 0) + 1

print(f"  平均邻居数: {total_nbrs/n_total:.2f}")
print(f"  最大邻居数: {max_nbrs}")
print(f"  最小邻居数: {min_nbrs}")
print(f"  邻居数分布:")
for count in sorted(nbr_hist.keys()):
    print(f"    {count:3d} 个邻居: {nbr_hist[count]:6d} 节点")

# ================================================================
# 6. 关键节点详细分析
# ================================================================
print(f"\n--- 6. 关键节点详细分析 ---")
key_nodes = [2176, 3752, 492, 2776, 4801, 882, 4009, 2837]
for nid in key_nodes:
    if nid >= n_total:
        continue
    pt_level = int(levels_arr[nid])
    print(f"  Node {nid:4d} (level={pt_level}):")
    for l in range(pt_level + 1):
        begin, end = neighbor_range(nid, l)
        neighbors = neighbors_arr[begin:end]
        valid = neighbors[neighbors >= 0]
        print(f"    layer {l}: {' '.join(str(n) for n in valid)} ({len(valid)} 个)")

# ================================================================
# 7. 孤立节点检查
# ================================================================
print(f"\n--- 7. 孤立节点检查 ---")
isolated = 0
for i in range(n_total):
    begin, end = neighbor_range(i, 0)
    count = int(np.sum(neighbors_arr[begin:end] >= 0))
    if count == 0:
        isolated += 1
print(f"  孤立节点总数: {isolated} / {n_total} ({100.0*isolated/n_total:.2f}%)")

# ================================================================
# 8. 搜索测试：对比 query[0] 的搜索结果
# ================================================================
print(f"\n--- 8. 搜索测试 (query[0], ef=5, k=50) ---")
query_vec = read_fvecs("c:/code/book/dataset/siftsmall_processed/query.bin")
gt_path = "c:/code/book/dataset/siftsmall_processed/groundtruth.bin"
with open(gt_path, 'rb') as f:
    gt_data = f.read()
gt_k = struct.unpack('i', gt_data[0:4])[0]
gt = []
pos = 4
for q in range(query_vec.shape[0]):
    gt.append(list(struct.unpack('i' * gt_k, gt_data[pos:pos+gt_k*4])))
    pos += gt_k * 4
gt = np.array(gt)

# FAISS search
D, I = index.search(query_vec[:1], 50)
print(f"  FAISS ef=5 top-50 ids:")
print(f"    {I[0].tolist()}")
recall = len(set(I[0][:10]) & set(gt[0][:10])) / 10
print(f"  FAISS Recall@10: {recall:.4f}")

print("\nFAISS 诊断完成！")