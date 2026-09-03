"""
gradient_boosting — 梯度提升

演示 GBDT（Gradient Boosting Decision Tree）：残差拟合、逐步加法模型、正则化。
纯 numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. 回归树桩（弱学习器）
# ════════════════════════════════════════════════════════════
class RegressionStump:
    """回归树桩——用均值预测的简单弱学习器"""
    def __init__(self):
        self.threshold = None
        self.feature = None
        self.left_val = None
        self.right_val = None

    def fit(self, X, residuals):
        """拟合残差"""
        n, m = X.shape
        best_err = float("inf")
        for f in range(m):
            thresholds = np.unique(X[:, f])
            for t in thresholds:
                mask = X[:, f] <= t
                if mask.sum() == 0 or (~mask).sum() == 0:
                    continue
                left_val = residuals[mask].mean()
                right_val = residuals[~mask].mean()
                err = np.sum((residuals[mask] - left_val) ** 2) + \
                      np.sum((residuals[~mask] - right_val) ** 2)
                if err < best_err:
                    best_err = err
                    self.feature = f
                    self.threshold = t
                    self.left_val = left_val
                    self.right_val = right_val

    def predict(self, X):
        preds = np.empty(len(X))
        mask = X[:, self.feature] <= self.threshold
        preds[mask] = self.left_val
        preds[~mask] = self.right_val
        return preds


# ════════════════════════════════════════════════════════════
# 2. GBDT 回归器
# ════════════════════════════════════════════════════════════
class GBDTRegressor:
    """梯度提升回归树"""
    def __init__(self, n_estimators=50, learning_rate=0.1, max_depth=1):
        self.n_estimators = n_estimators
        self.learning_rate = learning_rate
        self.max_depth = max_depth
        self.trees = []
        self.base_pred = None

    def fit(self, X, y):
        self.trees = []
        # 初始预测为均值
        self.base_pred = y.mean()
        residuals = y - self.base_pred

        for i in range(self.n_estimators):
            tree = RegressionStump()
            tree.fit(X, residuals)
            self.trees.append(tree)
            # 更新残差
            preds = tree.predict(X)
            residuals -= self.learning_rate * preds

            if (i + 1) % 10 == 0:
                current_pred = self.predict(X)
                loss = np.mean((y - current_pred) ** 2)
                print(f"    迭代 {i+1}/{self.n_estimators}，MSE = {loss:.4f}")

    def predict(self, X):
        preds = np.full(len(X), self.base_pred)
        for tree in self.trees:
            preds += self.learning_rate * tree.predict(X)
        return preds


# ════════════════════════════════════════════════════════════
# 3. 梯度提升演示
# ════════════════════════════════════════════════════════════
def demo_gradient_boosting():
    """演示 GBDT 逐步拟合过程"""
    np.random.seed(42)
    X = np.linspace(0, 10, 200).reshape(-1, 1)
    y = np.sin(X).ravel() + np.random.normal(0, 0.1, 200)

    gbdt = GBDTRegressor(n_estimators=50, learning_rate=0.1)
    gbdt.fit(X, y)
    y_pred = gbdt.predict(X)
    final_mse = np.mean((y - y_pred) ** 2)

    print(f"\n  GBDT 回归演示")
    print(f"  学习率: {gbdt.learning_rate}")
    print(f"  树数量: {gbdt.n_estimators}")
    print(f"  初始预测（均值）: {gbdt.base_pred:.4f}")
    print(f"  最终 MSE: {final_mse:.4f}\n")


# ════════════════════════════════════════════════════════════
# 4. 正则化效果演示
# ════════════════════════════════════════════════════════════
def demo_regularization():
    """演示学习率（shrinkage）对泛化的影响"""
    np.random.seed(42)
    X = np.random.randn(150, 3)
    y = X[:, 0] * 2 + X[:, 1] - 0.5 * X[:, 2] + np.random.normal(0, 0.3, 150)

    # 高学习率（可能过拟合）
    gbdt_high = GBDTRegressor(n_estimators=100, learning_rate=0.5)
    gbdt_high.fit(X, y)
    mse_high = np.mean((y - gbdt_high.predict(X)) ** 2)

    # 低学习率（带正则化）
    gbdt_low = GBDTRegressor(n_estimators=100, learning_rate=0.05)
    gbdt_low.fit(X, y)
    mse_low = np.mean((y - gbdt_low.predict(X)) ** 2)

    print("  正则化效果对比（学习率 shrinkage）")
    print(f"  高学习率 (0.5):  训练 MSE = {mse_high:.4f}（可能过拟合）")
    print(f"  低学习率 (0.05): 训练 MSE = {mse_low:.4f}（更稳健）\n")


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  梯度提升 — GBDT/残差拟合/正则化        ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_gradient_boosting()
    demo_regularization()

    print("[[OK]] 梯度提升演示完成")
