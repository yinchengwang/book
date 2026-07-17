"""
test_level_distribution.py

测试我们的层级分配逻辑是否正确。
"""

import numpy as np
import faiss

# 测试 FAISS 的层级分配
M = 16
efConstruction = 200

# 创建索引
index = faiss.IndexHNSWFlat(128, M, faiss.METRIC_L2)
index.hnsw.efConstruction = efConstruction
index.hnsw.set_default_probas(M, 1.0 / np.log(M))

# 检查 assign_probas
hnsw = index.hnsw
print(f"FAISS assign_probas (前10层):")
for i in range(10):
    try:
        proba = hnsw.assign_probas.at(i)
        print(f"  layer {i}: {proba:.6f}")
    except:
        break

# 计算累积概率
print("\n累积概率:")
cum_prob = 0.0
for i in range(10):
    try:
        proba = hnsw.assign_probas.at(i)
        cum_prob += proba
        print(f"  layer <= {i}: {cum_prob:.6f}")
    except:
        break

# 检查随机数采样
print("\n模拟随机层级采样 (10000次):")
np.random.seed(42)
levels = [0] * 10
for _ in range(10000):
    f = np.random.random()
    level = 0
    cum = 0.0
    for i in range(20):
        try:
            proba = hnsw.assign_probas.at(i)
            if f < proba:
                level = i
                break
            f -= proba
        except:
            break
    if level < 10:
        levels[level] += 1

for i, cnt in enumerate(levels):
    print(f"  level {i}: {cnt} ({100*cnt/10000:.2f}%)")
