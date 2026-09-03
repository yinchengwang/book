"""
ml_fundamentals — 机器学习基础

演示监督学习 vs 无监督学习、训练/测试集划分、过拟合与欠拟合。
纯 Python + numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np

# ════════════════════════════════════════════════════════════
# 1. 监督学习：线性回归演示
# ════════════════════════════════════════════════════════════
def demo_supervised():
    """监督学习：使用带标签数据训练回归模型"""
    np.random.seed(42)
    X = np.linspace(0, 10, 50)
    y = 2 * X + 1 + np.random.normal(0, 1, 50)  # y = 2x + 1 + noise

    # 最小二乘法拟合
    A = np.vstack([X, np.ones(len(X))]).T
    w, b = np.linalg.lstsq(A, y, rcond=None)[0]
    y_pred = w * X + b

    mse = np.mean((y - y_pred) ** 2)
    print(f"  监督学习（线性回归）示例")
    print(f"  真实: y = 2x + 1 + noise")
    print(f"  拟合: y = {w:.2f}x + {b:.2f}")
    print(f"  MSE: {mse:.4f}\n")


# ════════════════════════════════════════════════════════════
# 2. 无监督学习：K-Means 聚类
# ════════════════════════════════════════════════════════════
def demo_unsupervised():
    """无监督学习：无标签数据的 K-Means 聚类"""
    np.random.seed(42)
    # 生成 3 个簇的数据
    cluster1 = np.random.normal([2, 2], 0.5, (20, 2))
    cluster2 = np.random.normal([7, 7], 0.5, (20, 2))
    cluster3 = np.random.normal([2, 7], 0.5, (20, 2))
    X = np.vstack([cluster1, cluster2, cluster3])

    # 简单 K-Means 实现
    k = 3
    centroids = X[np.random.choice(len(X), k, replace=False)]
    for _ in range(10):
        dists = np.linalg.norm(X[:, None] - centroids[None], axis=2)
        labels = np.argmin(dists, axis=1)
        centroids = np.array([X[labels == i].mean(axis=0) for i in range(k)])

    print(f"  无监督学习（K-Means 聚类）示例")
    print(f"  样本数: {len(X)}, 聚类数: {k}")
    for i in range(k):
        count = np.sum(labels == i)
        print(f"  簇 {i+1}: {count} 个样本, 中心: ({centroids[i,0]:.2f}, {centroids[i,1]:.2f})")
    print()


# ════════════════════════════════════════════════════════════
# 3. 训练/测试集划分
# ════════════════════════════════════════════════════════════
def demo_train_test_split():
    """演示训练集与测试集划分的重要性"""
    np.random.seed(42)
    X = np.linspace(0, 10, 100)
    y = np.sin(X) + np.random.normal(0, 0.1, 100)

    # 手动划分
    indices = np.random.permutation(len(X))
    split = int(len(X) * 0.8)
    train_idx, test_idx = indices[:split], indices[split:]

    X_train, y_train = X[train_idx], y[train_idx]
    X_test, y_test = X[test_idx], y[test_idx]

    print("  训练/测试集划分")
    print(f"  总样本: {len(X)}, 训练集: {len(X_train)}, 测试集: {len(X_test)}")
    # 过拟合演示：高次多项式
    coeffs = np.polyfit(X_train, y_train, 15)
    poly = np.poly1d(coeffs)
    train_mse = np.mean((y_train - poly(X_train)) ** 2)
    test_mse = np.mean((y_test - poly(X_test)) ** 2)
    print(f"  高阶多项式（15次）拟合:")
    print(f"  训练集 MSE: {train_mse:.4f} (低) ← 过拟合")
    print(f"  测试集 MSE: {test_mse:.4f} (高) ← 泛化差")
    print()


# ════════════════════════════════════════════════════════════
# 4. 过拟合 vs 欠拟合
# ════════════════════════════════════════════════════════════
def demo_overfitting_underfitting():
    """演示过拟合（高方差）与欠拟合（高偏差）"""
    np.random.seed(42)
    X = np.linspace(0, 10, 20)
    y = np.sin(X) + np.random.normal(0, 0.2, 20)

    # 欠拟合：一次多项式（太简单）
    p1 = np.poly1d(np.polyfit(X, y, 1))
    mse1 = np.mean((y - p1(X)) ** 2)

    # 适当拟合：三次多项式
    p3 = np.poly1d(np.polyfit(X, y, 3))
    mse3 = np.mean((y - p3(X)) ** 2)

    # 过拟合：15次多项式（太复杂）
    p15 = np.poly1d(np.polyfit(X, y, 15))
    mse15 = np.mean((y - p15(X)) ** 2)

    print("  过拟合 vs 欠拟合")
    print(f"  欠拟合（1次）: MSE = {mse1:.4f}（高偏差，无法捕捉模式）")
    print(f"  适当（3次）  : MSE = {mse3:.4f}（好拟合）")
    print(f"  过拟合（15次）: MSE = {mse15:.4f}（低偏差但高方差）")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  机器学习基础 — 核心概念演示            ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_supervised()
    demo_unsupervised()
    demo_train_test_split()
    demo_overfitting_underfitting()

    print("[[OK]] ML 基础演示完成")
