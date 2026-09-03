"""
neural_network — 神经网络

演示 MLP（多层感知机）、反向传播算法和激活函数。
纯 numpy 实现，无外部深度学习框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. 激活函数
# ════════════════════════════════════════════════════════════
def sigmoid(x):
    return 1 / (1 + np.exp(-np.clip(x, -100, 100)))


def sigmoid_derivative(x):
    s = sigmoid(x)
    return s * (1 - s)


def relu(x):
    return np.maximum(0, x)


def relu_derivative(x):
    return (x > 0).astype(float)


def tanh(x):
    return np.tanh(x)


def tanh_derivative(x):
    return 1 - np.tanh(x) ** 2


# ════════════════════════════════════════════════════════════
# 2. MLP（多层感知机）
# ════════════════════════════════════════════════════════════
class MLP:
    """多层感知机——支持自定义隐藏层结构和激活函数"""
    def __init__(self, layers, activation="relu", lr=0.01):
        """
        layers: 列表，如 [2, 4, 1] 表示输入 2 维、隐藏层 4 维、输出 1 维
        """
        self.layers = layers
        self.lr = lr
        self.activation = activation
        self.activations = {
            "relu": (relu, relu_derivative),
            "sigmoid": (sigmoid, sigmoid_derivative),
            "tanh": (tanh, tanh_derivative),
        }
        self.act_fn, self.act_deriv = self.activations[activation]

        # 初始化参数
        self.params = {}
        for i in range(1, len(layers)):
            self.params[f"W{i}"] = np.random.randn(layers[i-1], layers[i]) * np.sqrt(2.0 / layers[i-1])
            self.params[f"b{i}"] = np.zeros((1, layers[i]))

        self.cache = {}  # 前向传播缓存

    def forward(self, X):
        """前向传播"""
        self.cache["A0"] = X
        for i in range(1, len(self.layers)):
            Z = self.cache[f"A{i-1}"] @ self.params[f"W{i}"] + self.params[f"b{i}"]
            self.cache[f"Z{i}"] = Z
            if i == len(self.layers) - 1:  # 输出层（线性）
                self.cache[f"A{i}"] = Z
            else:
                self.cache[f"A{i}"] = self.act_fn(Z)
        return self.cache[f"A{len(self.layers)-1}"]

    def backward(self, y):
        """反向传播（MSE Loss）"""
        m = y.shape[0]
        L = len(self.layers) - 1
        grads = {}

        # 输出层梯度
        dA = self.cache[f"A{L}"] - y  # MSE 梯度
        dZ = dA  # 输出层无激活

        for i in range(L, 0, -1):
            A_prev = self.cache[f"A{i-1}"]
            grads[f"dW{i}"] = (A_prev.T @ dZ) / m
            grads[f"db{i}"] = np.sum(dZ, axis=0, keepdims=True) / m
            if i > 1:
                dA = dZ @ self.params[f"W{i}"].T
                dZ = dA * self.act_deriv(self.cache[f"Z{i-1}"])

        return grads

    def update(self, grads):
        """梯度下降更新参数"""
        L = len(self.layers) - 1
        for i in range(1, L + 1):
            self.params[f"W{i}"] -= self.lr * grads[f"dW{i}"]
            self.params[f"b{i}"] -= self.lr * grads[f"db{i}"]

    def train(self, X, y, epochs=500, verbose=True):
        """训练网络"""
        losses = []
        for epoch in range(epochs):
            output = self.forward(X)
            loss = np.mean((output - y) ** 2)
            losses.append(loss)
            grads = self.backward(y)
            self.update(grads)
            if verbose and (epoch + 1) % 100 == 0:
                print(f"    Epoch {epoch+1}/{epochs}, Loss = {loss:.6f}")
        return losses


# ════════════════════════════════════════════════════════════
# 3. 激活函数演示
# ════════════════════════════════════════════════════════════
def demo_activations():
    """演示各种激活函数的形状和特性"""
    x = np.array([-5.0, -2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 5.0])

    print("  激活函数对比")
    print(f"  {'x':>6}", end="")
    for name in ["Sigmoid", "Tanh", "ReLU"]:
        print(f"  {name:>8}", end="")
    print()

    for val in x:
        s = sigmoid(val)
        t = tanh(val)
        r = relu(val)
        print(f"  {val:>6.1f}  {s:>8.4f}  {t:>8.4f}  {r:>8.4f}")
    print()

    # 激活函数特性
    print("  激活函数特性")
    print(f"  Sigmoid: 输出 (0,1)，适合二分类输出层。缺点：两端饱和梯度消失")
    print(f"  Tanh:    输出 (-1,1)，零中心化。缺点：两端仍饱和")
    print(f"  ReLU:    输出 [0,∞)，稀疏激活，减轻梯度消失。缺点：死神经元")
    print()


# ════════════════════════════════════════════════════════════
# 4. MLP 训练演示
# ════════════════════════════════════════════════════════════
def demo_mlp_training():
    """MLP 训练异或（XOR）问题"""
    np.random.seed(42)
    # XOR 问题——线性不可分
    X = np.array([[0, 0], [0, 1], [1, 0], [1, 1]])
    y = np.array([[0], [1], [1], [0]])

    print("  MLP 训练（XOR 问题——线性不可分）")
    print(f"  网络结构: 2 → 4 → 1（输入→隐藏→输出）")
    print(f"  激活函数: ReLU（隐藏层）")
    print(f"  学习率: 0.01")

    mlp = MLP(layers=[2, 4, 1], activation="relu", lr=0.01)
    losses = mlp.train(X, y, epochs=1000, verbose=True)

    predictions = mlp.forward(X)
    print(f"\n  预测结果（四舍五入）")
    for i, (x_i, y_i) in enumerate(zip(X, y)):
        pred = predictions[i, 0]
        correct = "(✓)" if abs(pred - y_i[0]) < 0.5 else "(✗)"
        print(f"    {x_i[0]} XOR {x_i[1]} = {y_i[0]} → 预测 {pred:.4f} {correct}")
    print()

    # 验证精度
    accuracy = np.mean((predictions > 0.5).astype(int).ravel() == y.ravel())
    print(f"  最终准确率: {accuracy * 100:.1f}%\n")


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  神经网络 — MLP/反向传播/激活函数       ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_activations()
    demo_mlp_training()

    print("[[OK]] 神经网络演示完成")
