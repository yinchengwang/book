"""
svm — 支持向量机

演示最大间隔分类、核函数（线性/RBF 映射）、软间隔与松弛变量。
纯 numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. 线性 SVM（原始形式——梯度下降求解）
# ════════════════════════════════════════════════════════════
class LinearSVM:
    """线性 SVM —— Hinge Loss + L2 正则化"""
    def __init__(self, C=1.0, lr=0.01, n_iter=500):
        self.C = C          # 正则化强度倒数
        self.lr = lr        # 学习率
        self.n_iter = n_iter
        self.w = None
        self.b = None

    def fit(self, X, y):
        n, m = X.shape
        y = np.where(y <= 0, -1, 1)  # 转换为 ±1
        self.w = np.zeros(m)
        self.b = 0.0

        for i in range(self.n_iter):
            # Hinge Loss 梯度
            margins = y * (X @ self.w + self.b)
            hinge_mask = margins < 1
            dw = self.w - self.C * (X[hinge_mask] * y[hinge_mask, None]).sum(axis=0)
            db = -self.C * y[hinge_mask].sum()
            # 非 hinge 样本：仅正则化项
            if (~hinge_mask).any():
                dw += (X[~hinge_mask] * 0).sum(axis=0)  # 无贡献
            dw /= n
            db /= n
            self.w -= self.lr * dw
            self.b -= self.lr * db

    def predict(self, X):
        return np.sign(X @ self.w + self.b)


# ════════════════════════════════════════════════════════════
# 2. 核函数
# ════════════════════════════════════════════════════════════
def linear_kernel(x1, x2):
    return np.dot(x1, x2)


def rbf_kernel(x1, x2, gamma=1.0):
    """RBF 高斯核函数 k(x,y) = exp(-γ||x-y||²)"""
    diff = x1 - x2
    return np.exp(-gamma * np.dot(diff, diff))


def polynomial_kernel(x1, x2, degree=3, coef0=1):
    """多项式核函数 k(x,y) = (x·y + coef0)^degree"""
    return (np.dot(x1, x2) + coef0) ** degree


# ════════════════════════════════════════════════════════════
# 3. 核函数演示
# ════════════════════════════════════════════════════════════
def demo_kernel_functions():
    """演示各种核函数的效果"""
    np.random.seed(42)
    x1 = np.array([1.0, 2.0])
    x2 = np.array([3.0, 4.0])

    linear_val = linear_kernel(x1, x2)
    rbf_val = rbf_kernel(x1, x2, gamma=0.5)
    poly_val = polynomial_kernel(x1, x2, degree=3)

    print("  核函数演示")
    print(f"  x1 = {x1}, x2 = {x2}")
    print(f"  线性核:        k(x1,x2) = {linear_val:.2f}")
    print(f"  RBF 核 (γ=0.5): k(x1,x2) = {rbf_val:.4f}")
    print(f"  多项式核 (d=3): k(x1,x2) = {poly_val:.2f}")
    print()

    # RBF 核的 gamma 参数影响
    points = np.linspace(-5, 5, 11)
    x_ref = np.array([0.0, 0.0])
    print("  RBF 核 γ 参数影响（距离原点 (0,0) 的相似度）")
    for gamma in [0.1, 1.0, 10.0]:
        sims = [rbf_kernel(np.array([p, p]), x_ref, gamma) for p in points]
        near = sims[len(sims) // 2]
        far = sims[-1]
        print(f"    γ={gamma:5.1f}: 原点相似度={near:.4f}, 远端相似度={far:.6f}")
    print()


# ════════════════════════════════════════════════════════════
# 4. 线性 SVM 分类演示
# ════════════════════════════════════════════════════════════
def demo_linear_svm():
    """线性 SVM 分类（软间隔）"""
    np.random.seed(42)
    # 生成线性可分但有噪声的数据
    X0 = np.random.normal([2, 2], 1.0, (50, 2))
    X1 = np.random.normal([6, 6], 1.2, (50, 2))
    X = np.vstack([X0, X1])
    y = np.hstack([-np.ones(50), np.ones(50)])

    # 训练 SVM
    svm = LinearSVM(C=1.0, lr=0.01, n_iter=500)
    svm.fit(X, y)
    preds = svm.predict(X)
    accuracy = np.mean(preds == y)

    print(f"  线性 SVM 分类")
    print(f"  正则化参数 C = {svm.C}")
    print(f"  权重向量: w = {svm.w}")
    print(f"  偏置: b = {svm.b:.4f}")
    print(f"  训练准确率: {accuracy * 100:.1f}%\n")


# ════════════════════════════════════════════════════════════
# 5. 软间隔演示
# ════════════════════════════════════════════════════════════
def demo_soft_margin():
    """演示不同 C 值对软间隔的影响"""
    np.random.seed(42)
    # 有重叠的数据
    X0 = np.random.normal([3, 3], 1.5, (40, 2))
    X1 = np.random.normal([5, 5], 1.5, (40, 2))
    X = np.vstack([X0, X1])
    y = np.hstack([-np.ones(40), np.ones(40)])

    print("  软间隔 — C 参数影响")
    for C in [0.01, 1.0, 100.0]:
        svm = LinearSVM(C=C, lr=0.01, n_iter=500)
        svm.fit(X, y)
        preds = svm.predict(X)
        acc = np.mean(preds == y)
        margin = 2.0 / np.linalg.norm(svm.w) if np.linalg.norm(svm.w) > 0 else float("inf")
        print(f"    C={C:6.2f}: 准确率={acc:.2f}, 间隔宽度={margin:.2f}"
              f"{'（窄间隔/硬边界）' if C > 10 else '（宽间隔/软边界）' if C < 0.1 else ''}")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  SVM — 最大间隔/核函数/软间隔           ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_kernel_functions()
    demo_linear_svm()
    demo_soft_margin()

    print("[[OK]] SVM 演示完成")
