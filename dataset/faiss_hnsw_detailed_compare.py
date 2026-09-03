"""
faiss_hnsw_detailed_compare.py

对前 N 个节点，逐层逐节点对比邻居列表。
输出格式：每个节点的每一层邻居都列出，对比两边差异。
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

# 读取 FAISS 索引
base_path = "c:/code/book/dataset/siftsmall_processed/base.bin"
base = read_fvecs(base_path)

M = 16
efConstruction = 200
index = faiss.IndexHNSWFlat(base.shape[1], M, faiss.METRIC_L2)
index.hnsw.efConstruction = efConstruction
index.hnsw.set_default_probas(M, 1.0 / np.log(M))
index.add(base)
hnsw = index.hnsw

# 获取 FAISS 数据
levels_arr = faiss.vector_to_array(hnsw.levels)
neighbors_arr = faiss.vector_to_array(hnsw.neighbors)
offsets_arr = faiss.vector_to_array(hnsw.offsets)
cum_arr = faiss.vector_to_array(hnsw.cum_nneighbor_per_level)

# 只取前 9 个 cum 值（实际有效层数）
# cum_arr 格式: [0, 32, 48, 64, 80, 96, 112, 128, 144, ...]
# 前9个是 level 0..8 的累积槽位数
cum_valid = cum_arr[:9]
print(f"FAISS cum_nneighbor_per_level (前9个): {cum_valid.tolist()}")
print(f"FAISS max_level: {hnsw.max_level}")
print(f"FAISS entry_point: {hnsw.entry_point}")

def faiss_neighbor_range(no, layer_no):
    """FAISS 的 neighbor range 计算"""
    o = offsets_arr[no]
    begin = int(o + cum_valid[layer_no])
    end = int(o + cum_valid[layer_no + 1])
    return begin, end

def get_faiss_neighbors(no, layer_no):
    """获取 FAISS 某个节点某层的有效邻居列表"""
    begin, end = faiss_neighbor_range(no, layer_no)
    neighbors = []
    for j in range(begin, end):
        nid = int(neighbors_arr[j])
        # FAISS 用最高位标记 null: 0x80000000
        if nid != 0x80000000 and nid >= 0:
            neighbors.append(nid)
    return neighbors

# 输出格式化的 FAISS 图信息
print("\n" + "="*70)
print("FAISS 图结构（关键节点）")
print("="*70)

# 层级分布
print("\n层级分布:")
for l in range(5):
    cnt = np.sum(levels_arr == l)
    print(f"  level {l}: {cnt} 节点")

# 关键节点详细邻居
key_nodes = [0, 1, 2, 3, 4, 5, 10, 11, 12, 2176, 3752, 492, 2776, 4801, 882, 4009, 2837]
for nid in key_nodes:
    if nid >= len(levels_arr):
        continue
    pt_level = int(levels_arr[nid])
    print(f"\nNode {nid:4d} (FAISS level={pt_level}):")
    for l in range(pt_level + 1):
        nbrs = get_faiss_neighbors(nid, l)
        print(f"  L{l}: {nbrs} ({len(nbrs)} 个)")

print("\n" + "="*70)
print("注意：保存此输出后，用 hnsw_graph_dump.c 输出的 '关键节点详细分析' 对比")
print("="*70)
