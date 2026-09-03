"""
node_by_node_compare.py

逐节点、逐层对比 FAISS 和我们的 HNSW 图结构。
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

# 只取前 9 个 cum 值
cum_valid = cum_arr[:9]

def faiss_neighbor_range(no, layer_no):
    o = offsets_arr[no]
    begin = int(o + cum_valid[layer_no])
    end = int(o + cum_valid[layer_no + 1])
    return begin, end

def get_faiss_neighbors(no, layer_no):
    begin, end = faiss_neighbor_range(no, layer_no)
    neighbors = []
    for j in range(begin, end):
        nid = int(neighbors_arr[j])
        if nid != 0x80000000 and nid >= 0:
            neighbors.append(nid)
    return neighbors

# 读取我们的实现输出
our_output = """
Node    0 (level=0):
  layer 0 [0..32): 2 6 8171 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (3 个有效邻居)
Node    1 (level=0):
  layer 0 [32..64): 3 14 7 3044 3071 2596 8778 4212 1258 3373 1591 4029 2813 2550 8264 2656 1539 1231 5 2786 4403 1754 115 9039 3763 [-] [-] [-] [-] [-] [-] [-] (25 个有效邻居)
Node    2 (level=0):
  layer 0 [64..96): 6 233 9256 474 9769 114 9040 3033 3912 3325 1288 9170 1959 5354 2995 3377 5297 5542 3762 4042 2426 324 960 5367 16 1483 9382 140 708 4 0 8171 (32 个有效邻居)
Node    3 (level=0):
  layer 0 [96..128): 1 8457 162 7398 5344 9104 3434 3293 6024 5373 3731 1792 4236 7047 3940 195 8839 1262 2550 1539 5 2786 1754 115 9039 [-] [-] [-] [-] [-] [-] [-] (25 个有效邻居)
Node    4 (level=0):
  layer 0 [128..160): 12 2 8827 140 383 1331 9046 8722 595 0 5833 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (11 个有效邻居)
Node    5 (level=0):
  layer 0 [160..192): 13 5633 1303 9308 8703 3 1329 4761 8334 9978 552 1477 6169 4880 3496 141 3235 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (17 个有效邻居)
Node    6 (level=0):
  layer 0 [192..224): 2 74 351 1153 1455 7431 8797 1911 7998 9773 1590 1635 2248 1270 5367 9135 8617 0 2661 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (19 个有效邻居)
Node    7 (level=0):
  layer 0 [224..256): 1 6517 2422 208 1640 6448 1655 8971 4806 307 195 2660 2656 9108 3183 8409 2786 9286 4403 1754 9413 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (21 个有效邻居)
Node    8 (level=0):
  layer 0 [256..288): 29 20 18 1270 962 3253 6225 960 1047 4773 374 2585 5886 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (13 个有效邻居)
Node    9 (level=0):
  layer 0 [288..320): 30 21 9103 75 86 8264 5113 2661 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (8 个有效邻居)
Node   10 (level=0):
  layer 0 [320..352): 8 3754 8176 6517 351 3253 1439 356 2083 3747 1857 9141 9382 18 4197 830 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (16 个有效邻居)
Node   11 (level=0):
  layer 0 [352..384): 19 30 1914 3252 8264 3755 7495 1587 2393 2656 7663 8475 7400 5229 9 4126 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (16 个有效邻居)
Node   12 (level=0):
  layer 0 [384..416): 1780 4914 6382 8722 2803 6262 2401 6168 7973 1804 3187 9046 8827 7687 1399 7127 1179 5063 2621 2523 4 6170 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (22 个有效邻居)
Node   13 (level=0):
  layer 0 [416..448): 5 9892 2266 1781 3371 545 968 1299 3235 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (9 个有效邻居)
Node   14 (level=0):
  layer 0 [448..480): 1 8457 162 8839 1434 278 855 1542 4951 500 4587 4945 3311 5846 2470 5548 1539 1231 5 2786 8865 2316 775 901 4403 1754 115 [-] [-] [-] [-] (27 个有效邻居)
Node   15 (level=0):
  layer 0 [480..512): 23 294 902 9950 2365 950 4773 2206 8654 5429 343 9244 3958 425 7127 3451 374 8382 824 1593 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (20 个有效邻居)
Node   16 (level=0):
  layer 0 [512..544): 27 5055 9880 621 3007 6277 372 3911 1021 4789 2568 1084 9847 1575 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (14 个有效邻居)
Node   17 (level=0):
  layer 0 [544..576): 28 2637 1083 5379 253 2184 9100 864 196 5702 4748 8552 1797 6383 4946 5944 1748 986 906 622 235 797 4960 4346 1733 714 2535 3036 8986 9134 2094 552 (32 个)
Node   18 (level=0):
  layer 0 [576..608): 10 8 3754 8176 74 7050 1946 3325 1915 830 1929 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (11 个有效邻居)
Node   19 (level=0):
  layer 0 [608..640): 11 21 3326 75 1820 1613 434 2654 1329 3358 4884 5899 1171 [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] [-] (13 个有效邻居)
"""

# 解析我们的邻居列表
def parse_our_neighbors(our_output):
    """从 hnsw_graph_dump.c 输出解析邻居"""
    our_nodes = {}
    lines = our_output.strip().split('\n')
    current_node = None

    for line in lines:
        line = line.strip()
        if line.startswith('Node'):
            parts = line.split()
            node_id = int(parts[1])
            current_node = node_id
            our_nodes[node_id] = {0: [], 1: [], 2: []}  # layer 0, 1, 2
        elif line.startswith('layer') and current_node is not None:
            # 解析: "layer 0 [0..32): 2 6 8171 ... (3 个有效邻居)"
            parts = line.split(':')
            layer_part = parts[0].strip()
            layer = int(layer_part.split()[1])

            # 提取数字邻居（排除 [-]）
            if len(parts) > 1:
                neighbor_part = parts[1].strip()
                numbers = []
                for token in neighbor_part.split():
                    token = token.rstrip(')')
                    if token != '[-]':
                        try:
                            numbers.append(int(token))
                        except ValueError:
                            pass
                our_nodes[current_node][layer] = numbers

    return our_nodes

our_nodes = parse_our_neighbors(our_output)

print("=" * 80)
print("逐节点逐层邻居对比")
print("=" * 80)

# 对比前 20 个节点
for node_id in range(20):
    print(f"\n--- Node {node_id} ---")

    # FAISS
    faiss_level = int(levels_arr[node_id])
    print(f"FAISS level={faiss_level}, Our level={node_id} in our_nodes={node_id in our_nodes}")

    faiss_l0 = get_faiss_neighbors(node_id, 0)
    faiss_l1 = get_faiss_neighbors(node_id, 1) if faiss_level >= 1 else []

    print(f"  FAISS L0 ({len(faiss_l0)}): {faiss_l0}")
    print(f"  FAISS L1 ({len(faiss_l1)}): {faiss_l1}")

    if node_id in our_nodes:
        our_l0 = our_nodes[node_id][0]
        our_l1 = our_nodes[node_id][1]
        print(f"  Our    L0 ({len(our_l0)}): {our_l0}")
        print(f"  Our    L1 ({len(our_l1)}): {our_l1}")

        # 计算交集
        if faiss_l0 and our_l0:
            common = set(faiss_l0) & set(our_l0)
            print(f"  L0 交集: {len(common)} / {max(len(faiss_l0), len(our_l0))} ({100*len(common)/max(len(faiss_l0), len(our_l0)):.1f}%)")
        if faiss_l1 and our_l1:
            common = set(faiss_l1) & set(our_l1)
            print(f"  L1 交集: {len(common)} / {max(len(faiss_l1), len(our_l1))} ({100*len(common)/max(len(faiss_l1), len(our_l1)):.1f}%)")
