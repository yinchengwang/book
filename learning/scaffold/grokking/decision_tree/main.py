"""
decision_tree — 决策树

演示 ID3 和 CART 算法、信息增益、基尼系数和剪枝。
纯 numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np
from collections import Counter


# ════════════════════════════════════════════════════════════
# 1. 信息增益计算
# ════════════════════════════════════════════════════════════
def entropy(y):
    """计算熵 H(y) = -Σ p_i * log₂(p_i)"""
    _, counts = np.unique(y, return_counts=True)
    probs = counts / len(y)
    return -np.sum(probs * np.log2(probs + 1e-10))


def gini(y):
    """计算基尼系数 Gini = 1 - Σ p_i²"""
    _, counts = np.unique(y, return_counts=True)
    probs = counts / len(y)
    return 1 - np.sum(probs ** 2)


def information_gain(X_col, y, criterion="entropy"):
    """计算按特征划分的信息增益"""
    parent_impurity = entropy(y) if criterion == "entropy" else gini(y)
    values, counts = np.unique(X_col, return_counts=True)
    weighted_child = 0
    for v, c in zip(values, counts):
        mask = X_col == v
        child_impurity = entropy(y[mask]) if criterion == "entropy" else gini(y[mask])
        weighted_child += (c / len(X_col)) * child_impurity
    return parent_impurity - weighted_child


# ════════════════════════════════════════════════════════════
# 2. 决策树节点
# ════════════════════════════════════════════════════════════
class DecisionNode:
    def __init__(self, feature=None, threshold=None, left=None, right=None, value=None):
        self.feature = feature    # 划分特征索引
        self.threshold = threshold  # 划分阈值
        self.left = left          # 左子树
        self.right = right        # 右子树
        self.value = value        # 叶节点类别


class DecisionTree:
    """CART 决策树（分类）"""
    def __init__(self, max_depth=3, min_samples_split=2):
        self.max_depth = max_depth
        self.min_samples_split = min_samples_split
        self.tree = None

    def _best_split(self, X, y):
        """找到最佳划分特征和阈值"""
        best_gain = -1
        best_feat, best_thresh = None, None
        n_feats = X.shape[1]
        for f in range(n_feats):
            thresholds = np.unique(X[:, f])
            for t in thresholds:
                gain = self._gini_gain(X[:, f], y, t)
                if gain > best_gain:
                    best_gain = gain
                    best_feat, best_thresh = f, t
        return best_feat, best_thresh

    def _gini_gain(self, X_col, y, threshold):
        parent_gini = gini(y)
        mask = X_col <= threshold
        n_left, n_right = mask.sum(), (~mask).sum()
        if n_left == 0 or n_right == 0:
            return 0
        left_gini = gini(y[mask])
        right_gini = gini(y[~mask])
        return parent_gini - (n_left / len(y) * left_gini + n_right / len(y) * right_gini)

    def _build(self, X, y, depth=0):
        n_samples, n_features = X.shape
        n_classes = len(np.unique(y))
        if (depth >= self.max_depth or n_samples < self.min_samples_split or n_classes == 1):
            return DecisionNode(value=Counter(y).most_common(1)[0][0])

        feat, thresh = self._best_split(X, y)
        if feat is None:
            return DecisionNode(value=Counter(y).most_common(1)[0][0])

        mask = X[:, feat] <= thresh
        left = self._build(X[mask], y[mask], depth + 1)
        right = self._build(X[~mask], y[~mask], depth + 1)
        return DecisionNode(feature=feat, threshold=thresh, left=left, right=right)

    def fit(self, X, y):
        self.tree = self._build(X, y)

    def _predict(self, x, node):
        if node.value is not None:
            return node.value
        if x[node.feature] <= node.threshold:
            return self._predict(x, node.left)
        return self._predict(x, node.right)

    def predict(self, X):
        return np.array([self._predict(x, self.tree) for x in X])


# ════════════════════════════════════════════════════════════
# 3. 决策树演示
# ════════════════════════════════════════════════════════════
def demo_decision_tree():
    """完整演示决策树的训练与剪枝"""
    np.random.seed(42)
    # 简单二维分类数据
    X = np.random.randn(200, 2) * 2
    y = (X[:, 0] ** 2 + X[:, 1] ** 2 < 4).astype(int).ravel()

    # 无剪枝（深度 10）
    dt_deep = DecisionTree(max_depth=10)
    dt_deep.fit(X, y)
    pred_deep = dt_deep.predict(X)
    acc_deep = np.mean(pred_deep == y)

    # 剪枝（深度 3）
    dt_pruned = DecisionTree(max_depth=3)
    dt_pruned.fit(X, y)
    pred_pruned = dt_pruned.predict(X)
    acc_pruned = np.mean(pred_pruned == y)

    print(f"  决策树演示（CART 算法）")
    print(f"  数据集: {len(X)} 个样本, 2 类")
    print(f"  无剪枝（深度 10）: 训练准确率 {acc_deep:.3f}（可能过拟合）")
    print(f"  剪枝（深度 3）  : 训练准确率 {acc_pruned:.3f}（泛化更好）\n")


# ════════════════════════════════════════════════════════════
# 4. ID3 信息增益演示
# ════════════════════════════════════════════════════════════
def demo_id3_information_gain():
    """演示 ID3 的信息增益计算"""
    # 经典的"打球"数据集（天气、温度、湿度、风）
    X = np.array([
        [0, 0, 0, 0], [0, 0, 0, 1], [1, 0, 0, 0],
        [2, 1, 0, 0], [2, 2, 1, 0], [2, 2, 1, 1],
        [1, 2, 1, 1], [0, 1, 0, 0], [0, 2, 1, 0],
        [2, 1, 1, 0], [0, 1, 1, 1], [1, 1, 0, 1],
        [1, 0, 1, 0], [2, 1, 0, 1],
    ])
    y = np.array([0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0])

    feature_names = ["天气", "温度", "湿度", "风"]
    gains = []
    for i in range(4):
        gain = information_gain(X[:, i], y, "entropy")
        gains.append(gain)

    sorted_feats = np.argsort(gains)[::-1]
    print("  ID3 信息增益计算")
    for idx in sorted_feats:
        print(f"  特征 {feature_names[idx]}: 信息增益 = {gains[idx]:.4f}")
    print(f"  最佳划分特征: {feature_names[sorted_feats[0]]}\n")


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  决策树 — ID3/CART/信息增益/剪枝        ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_id3_information_gain()
    demo_decision_tree()

    print("[[OK]] 决策树演示完成")
