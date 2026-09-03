"""
clustering — 聚类

演示 K-Means、层次聚类、DBSCAN 密度聚类。
纯 numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. K-Means 聚类
# ════════════════════════════════════════════════════════════
class KMeans:
    """K-Means 聚类实现"""
    def __init__(self, k=3, max_iter=100):
        self.k = k
        self.max_iter = max_iter
        self.centroids = None
        self.labels = None

    def fit(self, X):
        np.random.seed(42)
        # 随机初始化质心
        indices = np.random.choice(len(X), self.k, replace=False)
        self.centroids = X[indices].copy()

        for i in range(self.max_iter):
            # E 步：分配样本到最近质心
            dists = np.linalg.norm(X[:, None] - self.centroids[None], axis=2)
            self.labels = np.argmin(dists, axis=1)

            # M 步：更新质心
            new_centroids = np.array([X[self.labels == j].mean(axis=0)
                                       for j in range(self.k)])
            # 收敛检查
            if np.allclose(self.centroids, new_centroids):
                print(f"    K-Means 在第 {i+1} 轮收敛")
                break
            self.centroids = new_centroids

        # 计算 inertia（簇内平方和）
        inertia = sum(np.sum((X[self.labels == j] - self.centroids[j])**2)
                      for j in range(self.k))
        return inertia

    def predict(self, X):
        dists = np.linalg.norm(X[:, None] - self.centroids[None], axis=2)
        return np.argmin(dists, axis=1)


# ════════════════════════════════════════════════════════════
# 2. 层次聚类（自底向上凝聚）
# ════════════════════════════════════════════════════════════
class HierarchicalClustering:
    """凝聚式层次聚类——单链（最小距离）"""
    def __init__(self, n_clusters=3):
        self.n_clusters = n_clusters
        self.labels = None

    def fit(self, X):
        n = len(X)
        # 初始化每个点为一簇
        clusters = {i: [i] for i in range(n)}
        # 距离矩阵
        dist_matrix = np.linalg.norm(X[:, None] - X[None], axis=2)

        while len(clusters) > self.n_clusters:
            # 找最近的两个簇
            keys = list(clusters.keys())
            min_dist = float("inf")
            merge_pair = None
            for i in range(len(keys)):
                for j in range(i + 1, len(keys)):
                    ci, cj = keys[i], keys[j]
                    # 单链：簇间最小距离
                    d = min(dist_matrix[a][b] for a in clusters[ci] for b in clusters[cj])
                    if d < min_dist:
                        min_dist = d
                        merge_pair = (ci, cj)

            # 合并
            c1, c2 = merge_pair
            clusters[c1].extend(clusters[c2])
            del clusters[c2]

        # 生成标签
        self.labels = np.zeros(n, dtype=int)
        for label, (key, indices) in enumerate(clusters.items()):
            for idx in indices:
                self.labels[idx] = label

        return self.labels


# ════════════════════════════════════════════════════════════
# 3. DBSCAN（简化版）
# ════════════════════════════════════════════════════════════
class DBSCAN:
    """DBSCAN 密度聚类（简化实现）"""
    def __init__(self, eps=0.5, min_samples=5):
        self.eps = eps
        self.min_samples = min_samples
        self.labels = None

    def fit(self, X):
        n = len(X)
        self.labels = np.full(n, -1)  # -1 表示噪声
        # 距离矩阵
        dists = np.linalg.norm(X[:, None] - X[None], axis=2)
        # 找到每个点的邻域
        neighborhoods = [np.where(dists[i] <= self.eps)[0] for i in range(n)]
        # 核心点：邻域大小 ≥ min_samples
        core_points = [i for i in range(n) if len(neighborhoods[i]) >= self.min_samples]

        cluster_id = 0
        visited = set()

        for p in core_points:
            if p in visited:
                continue
            visited.add(p)
            # BFS 扩展簇
            queue = [p]
            cluster_points = []
            while queue:
                q = queue.pop(0)
                cluster_points.append(q)
                if len(neighborhoods[q]) >= self.min_samples:  # 核心点
                    for neighbor in neighborhoods[q]:
                        if neighbor not in visited:
                            visited.add(neighbor)
                            queue.append(neighbor)

            self.labels[cluster_points] = cluster_id
            cluster_id += 1

        n_noise = np.sum(self.labels == -1)
        n_clusters = cluster_id
        return n_clusters, n_noise


# ════════════════════════════════════════════════════════════
# 4. 聚类演示
# ════════════════════════════════════════════════════════════
def demo_clustering():
    """演示三种聚类算法"""
    np.random.seed(42)
    # 生成 3 个簇的数据
    clusters_data = [
        np.random.normal([2, 2], 0.5, (40, 2)),
        np.random.normal([7, 7], 0.5, (40, 2)),
        np.random.normal([7, 2], 0.5, (40, 2)),
    ]
    X = np.vstack(clusters_data)

    # K-Means
    print("  [K-Means 聚类]")
    kmeans = KMeans(k=3)
    inertia = kmeans.fit(X)
    _, counts = np.unique(kmeans.labels, return_counts=True)
    for i, c in enumerate(counts):
        print(f"    簇 {i+1}: {c} 个样本, 质心 ({kmeans.centroids[i,0]:.2f}, {kmeans.centroids[i,1]:.2f})")
    print(f"    Inertia（簇内平方和）: {inertia:.2f}\n")

    # 层次聚类
    print("  [层次聚类（单链）]")
    hc = HierarchicalClustering(n_clusters=3)
    hc_labels = hc.fit(X)
    _, hc_counts = np.unique(hc_labels, return_counts=True)
    for i, c in enumerate(hc_counts):
        print(f"    簇 {i+1}: {c} 个样本")
    sse_hc = sum(np.sum((X[hc_labels == i] - X[hc_labels == i].mean(axis=0)) ** 2)
                  for i in range(3))
    print(f"    SSE: {sse_hc:.2f}\n")

    # DBSCAN
    print("  [DBSCAN 密度聚类]")
    dbscan = DBSCAN(eps=0.8, min_samples=5)
    n_clusters, n_noise = dbscan.fit(X)
    print(f"    发现簇数: {n_clusters}")
    print(f"    噪声点: {n_noise}")
    _, db_counts = np.unique(dbscan.labels[dbscan.labels >= 0], return_counts=True)
    for i, c in enumerate(db_counts):
        print(f"    簇 {i+1}: {c} 个样本")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  聚类 — K-Means/层次聚类/DBSCAN         ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_clustering()

    print("[[OK]] 聚类演示完成")
