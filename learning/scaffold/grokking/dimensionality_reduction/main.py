"""
dimensionality_reduction — 降维

演示 PCA（主成分分析）、LDA（线性判别分析）、t-SNE 和 UMAP 思想。
纯 numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. PCA（主成分分析）
# ════════════════════════════════════════════════════════════
class PCA:
    """PCA 降维——通过特征值分解"""
    def __init__(self, n_components=2):
        self.n_components = n_components
        self.components = None
        self.mean = None
        self.explained_variance_ratio = None

    def fit(self, X):
        # 中心化
        self.mean = X.mean(axis=0)
        X_centered = X - self.mean
        # 协方差矩阵
        cov = np.cov(X_centered.T)
        # 特征值分解
        eigenvalues, eigenvectors = np.linalg.eigh(cov)
        # 按特征值降序排列
        idx = np.argsort(eigenvalues)[::-1]
        eigenvalues = eigenvalues[idx]
        eigenvectors = eigenvectors[:, idx]
        # 取前 n 个主成分
        self.components = eigenvectors[:, :self.n_components]
        # 解释方差比例
        total = eigenvalues.sum()
        self.explained_variance_ratio = eigenvalues[:self.n_components] / total
        return self

    def transform(self, X):
        X_centered = X - self.mean
        return X_centered @ self.components

    def fit_transform(self, X):
        self.fit(X)
        return self.transform(X)


# ════════════════════════════════════════════════════════════
# 2. LDA（线性判别分析）
# ════════════════════════════════════════════════════════════
class LDA:
    """LDA 降维——最大化类间/类内散度比"""
    def __init__(self, n_components=1):
        self.n_components = n_components
        self.components = None

    def fit(self, X, y):
        n_features = X.shape[1]
        classes = np.unique(y)
        # 全局均值
        mean_total = X.mean(axis=0)
        # 类内散度矩阵 Sw
        Sw = np.zeros((n_features, n_features))
        # 类间散度矩阵 Sb
        Sb = np.zeros((n_features, n_features))
        for c in classes:
            Xc = X[y == c]
            mean_c = Xc.mean(axis=0)
            # 类内散度
            Sw += (Xc - mean_c).T @ (Xc - mean_c)
            # 类间散度
            n_c = len(Xc)
            diff = (mean_c - mean_total).reshape(-1, 1)
            Sb += n_c * diff @ diff.T
        # 广义特征值问题：Sw⁻¹ @ Sb
        eigenvalues, eigenvectors = np.linalg.eigh(np.linalg.pinv(Sw) @ Sb)
        idx = np.argsort(eigenvalues)[::-1]
        self.components = eigenvectors[:, idx[:self.n_components]]
        return self

    def transform(self, X):
        return X @ self.components


# ════════════════════════════════════════════════════════════
# 3. t-SNE 思想演示（简化版）
# ════════════════════════════════════════════════════════════
def demo_tsne_concept():
    """演示 t-SNE 的核心思想：概率化降维"""
    np.random.seed(42)
    # 高维空间的距离
    X = np.random.randn(10, 50)
    dists = np.linalg.norm(X[:, None] - X[None], axis=2)
    sigma = 1.0
    # 高维空间概率 P（高斯核归一化）
    P = np.exp(-dists ** 2 / (2 * sigma ** 2))
    np.fill_diagonal(P, 0)
    P /= P.sum()

    # 低维空间概率 Q（t 分布）
    Y = np.random.randn(10, 2) * 0.1
    low_dists = np.linalg.norm(Y[:, None] - Y[None], axis=2)
    Q = 1 / (1 + low_dists ** 2)
    np.fill_diagonal(Q, 0)
    Q /= Q.sum()

    # KL 散度
    kl_div = np.sum(P * np.log(P / (Q + 1e-10) + 1e-10))
    print("  t-SNE 思想演示")
    print(f"  高维样本: 10 个 × 50 维")
    print(f"  低维投影: 10 个 × 2 维")
    print(f"  高斯核带宽 σ: {sigma}")
    print(f"  KL 散度（P||Q）: {kl_div:.4f}（越小越好）")
    print(f"  t-SNE 通过梯度下降最小化这个散度，保持局部邻居关系\n")


# ════════════════════════════════════════════════════════════
# 4. UMAP 思想演示（简化版）
# ════════════════════════════════════════════════════════════
def demo_umap_concept():
    """演示 UMAP 的核心思想：拓扑图降维"""
    np.random.seed(42)
    n = 30
    X = np.random.randn(n, 10)
    # 构建 kNN 图
    k = 5
    dists = np.linalg.norm(X[:, None] - X[None], axis=2)
    knn_indices = np.argsort(dists, axis=1)[:, 1:k+1]

    # 计算连通性
    adjacency = np.zeros((n, n))
    for i in range(n):
        for j in knn_indices[i]:
            adjacency[i, j] = 1
            adjacency[j, i] = 1

    # 低维嵌入空间的连通性
    Y = np.random.randn(n, 2) * 0.5
    low_dists = np.linalg.norm(Y[:, None] - Y[None], axis=2)
    low_adj = np.exp(-low_dists)
    np.fill_diagonal(low_adj, 0)

    # 交叉熵（UMAP 优化的目标）
    ce = np.sum(adjacency * np.log((adjacency + 1e-10) / (low_adj + 1e-10)) +
                (1 - adjacency) * np.log((1 - adjacency + 1e-10) / (1 - low_adj + 1e-10)))

    print("  UMAP 思想演示")
    print(f"  样本数: {n}，kNN k = {k}")
    print(f"  原始维度: 10，降维到: 2")
    print(f"  kNN 图边数: {adjacency.sum() // 2}")
    print(f"  交叉熵（当前随机嵌入）: {ce:.2f}（越小越好）")
    print(f"  UMAP 优化低维嵌入使得交叉熵最小\n")


# ════════════════════════════════════════════════════════════
# 5. PCA vs LDA 演示
# ════════════════════════════════════════════════════════════
def demo_pca_vs_lda():
    """比较 PCA（无监督）与 LDA（监督）的降维效果"""
    np.random.seed(42)
    # 生成 3 个类别的数据，有明显可分方向
    X0 = np.random.multivariate_normal([1, 0, 0], [[2, 0, 0], [0, 1, 0], [0, 0, 1]], 30)
    X1 = np.random.multivariate_normal([4, 3, 0], [[2, 0, 0], [0, 1, 0], [0, 0, 1]], 30)
    X2 = np.random.multivariate_normal([7, -3, 0], [[2, 0, 0], [0, 1, 0], [0, 0, 1]], 30)
    X = np.vstack([X0, X1, X2])
    y = np.hstack([np.zeros(30), np.ones(30), np.full(30, 2)])

    # PCA
    pca = PCA(n_components=2)
    X_pca = pca.fit_transform(X)
    # PCA 方差解释比
    var_ratio = pca.explained_variance_ratio
    print("  PCA vs LDA 降维对比")
    print(f"  数据集: 90 个样本, 3 类, 3 维")
    print(f"  PCA 前 2 维解释方差: {var_ratio[0]:.3f} + {var_ratio[1]:.3f} = {var_ratio.sum():.3f}")
    print(f"  PCA 寻找最大方差方向（无监督）——不关心类别标签")

    # LDA
    lda = LDA(n_components=2)
    try:
        X_lda = lda.fit_transform(X, y)
        print(f"  LDA 降维到 2 维（最多 C−1=2 维）")
    except Exception:
        try:
            lda2 = LDA(n_components=1)
            X_lda = lda2.fit_transform(X, y)
            print(f"  LDA 降维到 1 维")
        except Exception:
            print("  LDA 计算中遇到数值问题")
    print(f"  LDA 寻找类间分离方向（监督））
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  降维 — PCA/LDA/t-SNE/UMAP              ║")
    print("╚══════════════════════════════════════════╝\n")

    # PCA 演示
    print("  [PCA 主成分分析]")
    np.random.seed(42)
    X_high = np.random.randn(100, 10)
    pca = PCA(n_components=3)
    X_reduced = pca.fit_transform(X_high)
    cum_var = pca.explained_variance_ratio.sum()
    print(f"  原始维度: 10, 降维后: 3")
    print(f"  累计解释方差: {cum_var:.3f}")
    print(f"  各成分解释方差: {pca.explained_variance_ratio}\n")

    demo_pca_vs_lda()
    demo_tsne_concept()
    demo_umap_concept()

    print("[[OK]] 降维演示完成")
