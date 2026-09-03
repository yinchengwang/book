"""
logistic_regression — 逻辑回归

演示二分类、Sigmoid 函数、交叉熵损失和决策边界。
纯 numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np


def sigmoid(z):
    """Sigmoid 激活函数，将实数映射到 (0, 1) 概率区间"""
    return 1 / (1 + np.exp(-np.clip(z, -100, 100)))


# ════════════════════════════════════════════════════════════
# 1. 二分类数据生成
# ════════════════════════════════════════════════════════════
def demo_binary_classification():
    """生成线性可分的二分类数据"""
    np.random.seed(42)
    n_per_class = 50
    # 类别 0：中心在 (2, 2)
    X0 = np.random.normal([2, 2], 1.0, (n_per_class, 2))
    y0 = np.zeros(n_per_class)
    # 类别 1：中心在 (6, 6)
    X1 = np.random.normal([6, 6], 1.0, (n_per_class, 2))
    y1 = np.ones(n_per_class)

    X = np.vstack([X0, X1])
    y = np.hstack([y0, y1])
    print(f"  二分类数据集")
    print(f"  类别 0: {n_per_class} 个样本")
    print(f"  类别 1: {n_per_class} 个样本\n")
    return X, y


# ════════════════════════════════════════════════════════════
# 2. 逻辑回归训练（梯度下降）
# ════════════════════════════════════════════════════════════
def train_logistic_regression(X, y, lr=0.1, n_iter=500):
    """使用梯度下降训练逻辑回归模型"""
    m, n = X.shape
    X_b = np.c_[np.ones(m), X]  # 添加偏置项
    theta = np.zeros(n + 1)
    losses = []

    for i in range(n_iter):
        z = X_b @ theta
        h = sigmoid(z)
        # 交叉熵损失
        loss = -np.mean(y * np.log(h + 1e-10) + (1 - y) * np.log(1 - h + 1e-10))
        losses.append(loss)
        # 梯度
        gradient = (1 / m) * X_b.T @ (h - y)
        theta -= lr * gradient

    return theta, losses


# ════════════════════════════════════════════════════════════
# 3. Sigmoid 与决策边界演示
# ════════════════════════════════════════════════════════════
def demo_sigmoid_decision():
    """演示 Sigmoid 函数和决策边界"""
    z_vals = np.linspace(-10, 10, 100)
    probs = sigmoid(z_vals)

    print(f"  Sigmoid 函数特性")
    print(f"  z = -10 → P = {sigmoid(-10):.6f}（趋近 0）")
    print(f"  z = -2  → P = {sigmoid(-2):.4f}")
    print(f"  z =  0  → P = {sigmoid(0):.4f}（决策阈值）")
    print(f"  z =  2  → P = {sigmoid(2):.4f}")
    print(f"  z =  10 → P = {sigmoid(10):.6f}（趋近 1）")
    print()

    # 训练逻辑回归
    X_demo, y_demo = demo_binary_classification()
    theta, losses = train_logistic_regression(X_demo, y_demo)

    # 预测和准确率
    X_b = np.c_[np.ones(len(X_demo)), X_demo]
    probs = sigmoid(X_b @ theta)
    preds = (probs >= 0.5).astype(int)
    accuracy = np.mean(preds == y_demo)

    print(f"  交叉熵损失")
    print(f"  初始损失: {losses[0]:.4f}")
    print(f"  最终损失: {losses[-1]:.4f}")
    print(f"  训练准确率: {accuracy * 100:.1f}%")
    print(f"  决策边界: {theta[0]:.2f} + {theta[1]:.2f}*x1 + {theta[2]:.2f}*x2 = 0\n")


# ════════════════════════════════════════════════════════════
# 4. 交叉熵损失可视化
# ════════════════════════════════════════════════════════════
def demo_cross_entropy():
    """展示交叉熵损失函数的形状"""
    y_true = np.array([0, 1])
    preds = np.linspace(0.001, 0.999, 100)

    loss_when_y0 = -np.log(1 - preds)
    loss_when_y1 = -np.log(preds)

    print("  交叉熵损失（单个样本）")
    print(f"  y=1, P=0.9 → loss = {-np.log(0.9):.4f}")
    print(f"  y=1, P=0.1 → loss = {-np.log(0.1):.4f}（严重错判）")
    print(f"  y=0, P=0.9 → loss = {-np.log(0.1):.4f}（严重错判）")
    print(f"  y=0, P=0.1 → loss = {-np.log(0.9):.4f}")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  逻辑回归 — 二分类/Sigmoid/交叉熵       ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_sigmoid_decision()
    demo_cross_entropy()

    print("[[OK]] 逻辑回归演示完成")
