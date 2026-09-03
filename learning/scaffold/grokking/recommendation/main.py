"""
recommendation — 推荐系统

演示协同过滤（User-Based / Item-Based）、矩阵分解（SVD）和评估指标。
纯 numpy 实现，无外部 ML 框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. 评分矩阵
# ════════════════════════════════════════════════════════════
def build_rating_matrix():
    """构建用户-物品评分矩阵（1-5 星，0 表示未评分）"""
    R = np.array([
        [5, 3, 0, 1, 4, 0, 0],
        [4, 0, 0, 1, 5, 2, 0],
        [1, 1, 0, 5, 0, 4, 3],
        [0, 0, 4, 4, 0, 0, 5],
        [0, 3, 5, 0, 2, 5, 0],
    ])
    return R


# ════════════════════════════════════════════════════════════
# 2. 用户-用户协同过滤
# ════════════════════════════════════════════════════════════
class UserBasedCF:
    """基于用户的协同过滤"""
    def __init__(self, R, k=2):
        self.R = R
        self.n_users, self.n_items = R.shape
        self.k = k
        # 计算用户相似度矩阵（皮尔逊相关系数）
        self.similarity = self._compute_similarity()

    def _compute_similarity(self):
        """计算用户间相似度"""
        sim = np.zeros((self.n_users, self.n_users))
        for i in range(self.n_users):
            for j in range(i + 1, self.n_users):
                # 共同评分的物品
                common = (self.R[i] > 0) & (self.R[j] > 0)
                if common.sum() < 2:
                    sim[i, j] = sim[j, i] = 0
                    continue
                ri = self.R[i][common]
                rj = self.R[j][common]
                # 皮尔逊相关系数
                ri_mean = ri.mean()
                rj_mean = rj.mean()
                num = ((ri - ri_mean) * (rj - rj_mean)).sum()
                den = np.sqrt(((ri - ri_mean) ** 2).sum() * ((rj - rj_mean) ** 2).sum())
                sim[i, j] = sim[j, i] = num / den if den > 0 else 0
        return sim

    def predict(self, user_id, item_id):
        """预测用户对物品的评分"""
        if self.R[user_id, item_id] > 0:
            return self.R[user_id, item_id]
        # 找与目标用户最相似的 k 个且评分过该物品的用户
        rated_users = np.where(self.R[:, item_id] > 0)[0]
        if len(rated_users) == 0:
            return self.R[user_id][self.R[user_id] > 0].mean()
        sims = [(u, self.similarity[user_id, u]) for u in rated_users]
        sims.sort(key=lambda x: -abs(x[1]))
        top_k = sims[:self.k]
        sim_sum = sum(abs(s) for _, s in top_k)
        if sim_sum == 0:
            return self.R[user_id][self.R[user_id] > 0].mean()
        # 加权平均
        user_mean = self.R[user_id][self.R[user_id] > 0].mean()
        pred = user_mean
        for u, s in top_k:
            u_mean = self.R[u][self.R[u] > 0].mean()
            pred += s * (self.R[u, item_id] - u_mean) / sim_sum
        return max(1, min(5, pred))


# ════════════════════════════════════════════════════════════
# 3. 矩阵分解（SVD 类）
# ════════════════════════════════════════════════════════════
class MatrixFactorization:
    """矩阵分解——SVD 风格的协同过滤"""
    def __init__(self, n_factors=3, lr=0.01, reg=0.02, n_iter=500):
        self.n_factors = n_factors
        self.lr = lr
        self.reg = reg
        self.n_iter = n_iter
        self.P = None  # 用户因子矩阵
        self.Q = None  # 物品因子矩阵
        self.global_mean = None

    def fit(self, R):
        n_users, n_items = R.shape
        self.global_mean = R[R > 0].mean()
        # 初始化
        self.P = np.random.normal(0, 0.1, (n_users, self.n_factors))
        self.Q = np.random.normal(0, 0.1, (n_items, self.n_factors))

        # 训练（SGD）
        users, items = np.where(R > 0)
        n_ratings = len(users)
        for epoch in range(self.n_iter):
            np.random.seed(42 + epoch)
            order = np.random.permutation(n_ratings)
            mse = 0
            for idx in order:
                u = users[idx]
                i = items[idx]
                r_ui = R[u, i]
                pred = self.global_mean + self.P[u] @ self.Q[i]
                err = r_ui - pred
                mse += err ** 2
                # 更新
                self.P[u] += self.lr * (err * self.Q[i] - self.reg * self.P[u])
                self.Q[i] += self.lr * (err * self.P[u] - self.reg * self.Q[i])

            if (epoch + 1) % 100 == 0:
                print(f"    迭代 {epoch+1}/{self.n_iter}, MSE = {mse/n_ratings:.4f}")

    def predict(self, user_id, item_id):
        """预测评分"""
        return self.global_mean + self.P[user_id] @ self.Q[item_id]


# ════════════════════════════════════════════════════════════
# 4. 推荐演示
# ════════════════════════════════════════════════════════════
def demo_collaborative_filtering():
    """协同过滤推荐演示"""
    np.random.seed(42)
    R = build_rating_matrix()
    print("  用户-物品评分矩阵（5 用户 × 7 物品，0 表示未评分）")
    print(f"  {R}\n")

    # User-Based CF
    print("  [User-Based 协同过滤]")
    ucf = UserBasedCF(R, k=2)
    print(f"  用户相似度矩阵:")
    for i in range(5):
        sims = [f"{ucf.similarity[i,j]:.2f}" for j in range(5)]
        print(f"    用户 {i+1}: [{', '.join(sims)}]")

    # 预测未评分物品
    print(f"\n  用户 1 的评分预测:")
    for item in range(7):
        if R[0, item] == 0:
            pred = ucf.predict(0, item)
            print(f"    物品 {item+1} → 预测评分: {pred:.2f}")

    # 为用户 1 推荐 Top-2
    unrated = [(i, ucf.predict(0, i)) for i in range(7) if R[0, i] == 0]
    unrated.sort(key=lambda x: -x[1])
    print(f"\n  为用户 1 的 Top-2 推荐:")
    for item, score in unrated[:2]:
        print(f"    物品 {item+1}（预测评分: {score:.2f}）")
    print()


# ════════════════════════════════════════════════════════════
# 5. 矩阵分解演示
# ════════════════════════════════════════════════════════════
def demo_matrix_factorization():
    """矩阵分解推荐演示"""
    np.random.seed(42)
    R = build_rating_matrix()

    print("  [矩阵分解（SGD）]")
    print(f"  因子数: 3, 学习率: 0.01, 正则化: 0.02")
    mf = MatrixFactorization(n_factors=3, lr=0.01, reg=0.02, n_iter=300)
    mf.fit(R)

    # 重构矩阵
    R_pred = mf.global_mean + mf.P @ mf.Q.T
    print(f"\n  重构矩阵:")
    print(f"  {np.round(R_pred, 2)}\n")

    # 比较已知评分的预测误差
    known_mask = R > 0
    mae = np.abs(R[known_mask] - R_pred[known_mask]).mean()
    print(f"  已知评分 MAE: {mae:.4f}")

    # 推荐
    print(f"\n  为用户 3 的 Top-3 推荐:")
    unrated = [(i, mf.predict(2, i)) for i in range(7) if R[2, i] == 0]
    unrated.sort(key=lambda x: -x[1])
    for item, score in unrated[:3]:
        print(f"    物品 {item+1}（预测评分: {score:.2f}）")
    print()


# ════════════════════════════════════════════════════════════
# 6. 推荐评估指标
# ════════════════════════════════════════════════════════════
def demo_metrics():
    """推荐系统常用评估指标"""
    # 真实评分 vs 预测评分
    y_true = np.array([5, 4, 3, 2, 1])
    y_pred = np.array([4.5, 4.2, 3.8, 2.5, 1.2])

    # 用户对 10 个物品的真实兴趣（top-5 相关）
    relevant = {0, 1, 2, 4}  # 用户真正喜欢的物品 ID
    recommended = {0, 2, 3, 6, 7}  # 推荐系统给出的 Top-5

    tp = len(relevant & recommended)
    fp = len(recommended - relevant)
    fn = len(relevant - recommended)

    precision = tp / (tp + fp) if (tp + fp) > 0 else 0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0
    f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0

    mae = np.mean(np.abs(y_true - y_pred))
    rmse = np.sqrt(np.mean((y_true - y_pred) ** 2))

    print("  推荐系统评估指标")
    print(f"  真实 Top-4 偏好: {sorted(relevant)}")
    print(f"  推荐 Top-5: {sorted(recommended)}")
    print(f"  精准率 Precision: {precision:.2f}（推荐了多少正确的）")
    print(f"  召回率 Recall: {recall:.2f}（正确推荐了多少相关的）")
    print(f"  F1 值: {f1:.2f}（精确率和召回率的调和平均）")
    print(f"  MAE: {mae:.2f}, RMSE: {rmse:.2f}（评分预测误差）")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  推荐系统 — 协同过滤/矩阵分解          ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_collaborative_filtering()
    demo_matrix_factorization()
    demo_metrics()

    print("[[OK]] 推荐系统演示完成")
