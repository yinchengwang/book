"""
random_forest — 随机森林

演示 Bagging 集成、特征随机抽样和并行训练思想。
纯 numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np
from collections import Counter


# ════════════════════════════════════════════════════════════
# 1. 决策树桩（弱学习器）
# ════════════════════════════════════════════════════════════
class DecisionStump:
    """单层决策树桩——仅在一个特征上划分"""
    def __init__(self):
        self.feature = None
        self.threshold = None
        self.left_val = None
        self.right_val = None
        self.error = None

    def fit(self, X, y, sample_weight=None):
        n, m = X.shape
        if sample_weight is None:
            sample_weight = np.ones(n) / n
        best_err = float("inf")
        for f in range(m):
            thresholds = np.unique(X[:, f])
            for t in thresholds:
                mask = X[:, f] <= t
                left_weight = sample_weight[mask].sum()
                right_weight = sample_weight[~mask].sum()
                if left_weight == 0 or right_weight == 0:
                    continue
                # 每个叶子的预测值（加权投票）
                left_labels = y[mask]
                right_labels = y[~mask]
                left_pred = 1 if left_weight > 0 and np.average(left_labels, weights=sample_weight[mask]) > 0.5 else 0
                right_pred = 1 if right_weight > 0 and np.average(right_labels, weights=sample_weight[~mask]) > 0.5 else 0
                # 加权误差
                err = sample_weight[mask][left_labels != left_pred].sum() + \
                      sample_weight[~mask][right_labels != right_pred].sum()
                if err < best_err:
                    best_err = err
                    self.feature = f
                    self.threshold = t
                    self.left_val = left_pred
                    self.right_val = right_pred
        return self

    def predict(self, X):
        mask = X[:, self.feature] <= self.threshold
        preds = np.empty(len(X), dtype=int)
        preds[mask] = self.left_val
        preds[~mask] = self.right_val
        return preds


# ════════════════════════════════════════════════════════════
# 2. 随机森林
# ════════════════════════════════════════════════════════════
class RandomForest:
    """随机森林分类器（简化版——决策树桩集成）"""
    def __init__(self, n_estimators=50, max_features="sqrt"):
        self.n_estimators = n_estimators
        self.max_features = max_features
        self.trees = []

    def fit(self, X, y):
        self.trees = []
        n, m = X.shape
        n_features = int(np.sqrt(m)) if self.max_features == "sqrt" else m

        for i in range(self.n_estimators):
            # Bootstrap 抽样
            indices = np.random.choice(n, n, replace=True)
            X_boot, y_boot = X[indices], y[indices]
            # 特征随机抽样
            features = np.random.choice(m, n_features, replace=False)
            X_sub = X_boot[:, features]
            tree = DecisionStump()
            tree.fit(X_sub, y_boot)
            self.trees.append((tree, features))
        print(f"  随机森林训练完成: {self.n_estimators} 棵决策树")
        print(f"  每棵树特征数: {n_features}\n")

    def predict(self, X):
        preds = np.zeros((len(X), self.n_estimators))
        for i, (tree, features) in enumerate(self.trees):
            preds[:, i] = tree.predict(X[:, features])
        return np.round(preds.mean(axis=1)).astype(int)


# ════════════════════════════════════════════════════════════
# 3. Bagging 效果演示
# ════════════════════════════════════════════════════════════
def demo_bagging():
    """演示 Bagging 如何降低方差"""
    np.random.seed(42)
    X = np.random.randn(300, 5)
    y = (X[:, 0] + X[:, 1] > 0).astype(int)

    # 单棵决策树 vs 随机森林
    single_tree = DecisionStump()
    single_tree.fit(X, y)
    single_acc = np.mean(single_tree.predict(X) == y)

    rf = RandomForest(n_estimators=30)
    rf.fit(X, y)
    rf_acc = np.mean(rf.predict(X) == y)

    print("  Bagging 集成效果对比")
    print(f"  单棵决策树准确率: {single_acc:.3f}（高方差）")
    print(f"  随机森林（30 棵）: {rf_acc:.3f}（低方差，更稳定）")
    print(f"  提升幅度: +{(rf_acc - single_acc) * 100:.1f}%\n")


# ════════════════════════════════════════════════════════════
# 4. 特征重要性评估
# ════════════════════════════════════════════════════════════
def demo_feature_importance():
    """通过特征使用频率评估特征重要性"""
    np.random.seed(42)
    X = np.random.randn(500, 8)
    # 只有前 3 个特征有效
    y = (X[:, 0] * 2 + X[:, 1] - X[:, 2] > 0).astype(int)

    rf = RandomForest(n_estimators=50)
    rf.fit(X, y)

    feature_counts = np.zeros(8)
    for tree, features in rf.trees:
        feature_counts[features[tree.feature]] += 1
    feature_imp = feature_counts / feature_counts.sum()

    print("  特征重要性（基于使用频率）")
    for i, imp in enumerate(feature_imp):
        marker = " ★" if i < 3 else ""
        print(f"  特征 {i}: {imp:.3f}{marker}")
    print(f"  前 3 个特征（有效特征）累计占比: {feature_imp[:3].sum():.3f}")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  随机森林 — Bagging/特征抽样/集成       ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_bagging()
    demo_feature_importance()

    print("[[OK]] 随机森林演示完成")
