"""
linear_regression — 线性回归

演示一元线性回归、多元线性回归、梯度下降优化和评估指标。
纯 numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. 一元线性回归
# ════════════════════════════════════════════════════════════
def demo_univariate():
    """一元线性回归：y = wx + b，最小二乘法"""
    np.random.seed(42)
    X = np.linspace(0, 10, 100)
    y = 3 * X + 7 + np.random.normal(0, 1.5, 100)

    # 闭式解（最小二乘法）
    A = np.vstack([X, np.ones(len(X))]).T
    w, b = np.linalg.lstsq(A, y, rcond=None)[0]
    y_pred = w * X + b

    r2 = 1 - np.sum((y - y_pred) ** 2) / np.sum((y - y.mean()) ** 2)
    print(f"  一元线性回归（最小二乘法）")
    print(f"  真实: y = 3x + 7 + noise")
    print(f"  拟合: y = {w:.2f}x + {b:.2f}")
    print(f"  R² 决定系数: {r2:.4f}\n")


# ════════════════════════════════════════════════════════════
# 2. 多元线性回归
# ════════════════════════════════════════════════════════════
def demo_multivariate():
    """多元线性回归：y = w1x1 + w2x2 + b"""
    np.random.seed(42)
    n = 200
    X = np.random.normal(0, 1, (n, 3))  # 3 个特征
    true_w = np.array([2.5, -1.3, 3.7])
    true_b = 0.5
    y = X @ true_w + true_b + np.random.normal(0, 0.5, n)

    # 闭式解
    Xb = np.c_[X, np.ones(n)]
    theta = np.linalg.lstsq(Xb, y, rcond=None)[0]
    y_pred = Xb @ theta

    mae = np.mean(np.abs(y - y_pred))
    rmse = np.sqrt(np.mean((y - y_pred) ** 2))
    print(f"  多元线性回归（3 个特征）")
    print(f"  真实权重: {true_w}")
    print(f"  估计权重: {theta[:3]}")
    print(f"  截距: {theta[3]:.2f}")
    print(f"  MAE: {mae:.4f}, RMSE: {rmse:.4f}\n")


# ════════════════════════════════════════════════════════════
# 3. 梯度下降实现
# ════════════════════════════════════════════════════════════
def demo_gradient_descent():
    """梯度下降优化线性回归参数"""
    np.random.seed(42)
    X = np.linspace(0, 5, 50)
    y = 2 * X + 1 + np.random.normal(0, 0.5, 50)

    w, b = 0.0, 0.0
    lr = 0.05
    n_iter = 100
    n = len(X)
    losses = []

    for i in range(n_iter):
        y_pred = w * X + b
        loss = np.mean((y_pred - y) ** 2) / 2
        losses.append(loss)
        # 梯度
        dw = np.mean((y_pred - y) * X)
        db = np.mean(y_pred - y)
        # 更新
        w -= lr * dw
        b -= lr * db

    print(f"  梯度下降优化（{n_iter} 轮）")
    print(f"  初始损失: {losses[0]:.4f}, 最终损失: {losses[-1]:.4f}")
    print(f"  学习率: {lr}, 迭代: {n_iter}")
    print(f"  拟合结果: y = {w:.2f}x + {b:.2f}\n")


# ════════════════════════════════════════════════════════════
# 4. 评估指标
# ════════════════════════════════════════════════════════════
def demo_metrics():
    """回归评估指标：MSE、RMSE、MAE、R²"""
    np.random.seed(42)
    y_true = np.array([3.0, -0.5, 2.0, 7.0, 4.2])
    y_pred = np.array([2.8, -0.3, 2.2, 6.8, 4.0])

    mse = np.mean((y_true - y_pred) ** 2)
    rmse = np.sqrt(mse)
    mae = np.mean(np.abs(y_true - y_pred))
    r2 = 1 - np.sum((y_true - y_pred) ** 2) / np.sum((y_true - y_true.mean()) ** 2)
    mape = np.mean(np.abs((y_true - y_pred) / y_true)) * 100

    print("  回归评估指标")
    print(f"  MSE : {mse:.4f}（均方误差）")
    print(f"  RMSE: {rmse:.4f}（均方根误差）")
    print(f"  MAE : {mae:.4f}（平均绝对误差）")
    print(f"  MAPE: {mape:.2f}%（平均绝对百分比误差）")
    print(f"  R²  : {r2:.4f}（决定系数）")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  线性回归 — 一元/多元/梯度下降/评估     ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_univariate()
    demo_multivariate()
    demo_gradient_descent()
    demo_metrics()

    print("[[OK]] 线性回归演示完成")
