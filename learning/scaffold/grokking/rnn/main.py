"""
rnn — 循环神经网络

演示 RNN 序列建模、LSTM 记忆单元、GRU 简化门控、时序预测。
纯 numpy 实现，无外部深度学习框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. 简单 RNN 单元
# ════════════════════════════════════════════════════════════
class SimpleRNN:
    """简单 RNN：h_t = tanh(W_hh @ h_{t-1} + W_xh @ x_t + b_h)"""
    def __init__(self, input_size, hidden_size):
        self.input_size = input_size
        self.hidden_size = hidden_size
        # 参数初始化
        self.W_xh = np.random.randn(input_size, hidden_size) * 0.01
        self.W_hh = np.random.randn(hidden_size, hidden_size) * 0.01
        self.b_h = np.zeros((1, hidden_size))

    def forward(self, x_seq):
        """x_seq: (seq_len, input_size)"""
        seq_len = len(x_seq)
        h = np.zeros((1, self.hidden_size))
        outputs = []
        for t in range(seq_len):
            h = np.tanh(x_seq[t:t+1] @ self.W_xh + h @ self.W_hh + self.b_h)
            outputs.append(h.copy())
        return np.vstack(outputs), h


# ════════════════════════════════════════════════════════════
# 2. LSTM 单元
# ════════════════════════════════════════════════════════════
class LSTMCell:
    """LSTM 记忆单元"""
    def __init__(self, input_size, hidden_size):
        self.input_size = input_size
        self.hidden_size = hidden_size
        # 输入门、遗忘门、输出门、候选记忆（合并为 4×）
        combined = hidden_size * 4
        self.W_x = np.random.randn(input_size, combined) * 0.01
        self.W_h = np.random.randn(hidden_size, combined) * 0.01
        self.b = np.zeros((1, combined))

    def forward(self, x, h_prev, c_prev):
        """单步前向传播"""
        gates = x @ self.W_x + h_prev @ self.W_h + self.b
        i, f, o, g = np.split(gates, 4, axis=1)
        i = 1 / (1 + np.exp(-i))  # 输入门
        f = 1 / (1 + np.exp(-f))  # 遗忘门
        o = 1 / (1 + np.exp(-o))  # 输出门
        g = np.tanh(g)            # 候选记忆
        c = f * c_prev + i * g    # 细胞状态更新
        h = o * np.tanh(c)        # 隐藏状态
        return h, c


def lstm_forward(lstm, x_seq):
    """LSTM 多步前向传播"""
    seq_len = len(x_seq)
    h = np.zeros((1, lstm.hidden_size))
    c = np.zeros((1, lstm.hidden_size))
    outputs = []
    for t in range(seq_len):
        h, c = lstm.forward(x_seq[t:t+1], h, c)
        outputs.append(h.copy())
    return np.vstack(outputs), (h, c)


# ════════════════════════════════════════════════════════════
# 3. GRU 单元
# ════════════════════════════════════════════════════════════
class GRUCell:
    """GRU 门控循环单元"""
    def __init__(self, input_size, hidden_size):
        self.input_size = input_size
        self.hidden_size = hidden_size
        # 重置门 + 更新门 + 候选隐藏（3×）
        combined = hidden_size * 3
        self.W_x = np.random.randn(input_size, combined) * 0.01
        self.W_h = np.random.randn(hidden_size, combined) * 0.01
        self.b = np.zeros((1, combined))

    def forward(self, x, h_prev):
        """单步前向传播"""
        zr = x @ self.W_x + h_prev @ self.W_h + self.b
        z, r, n = np.split(zr, 3, axis=1)
        z = 1 / (1 + np.exp(-z))  # 更新门
        r = 1 / (1 + np.exp(-r))  # 重置门
        n = np.tanh(r * np.tanh(h_prev) + n)  # 候选隐藏（简化）
        h = (1 - z) * h_prev + z * n
        return h


# ════════════════════════════════════════════════════════════
# 4. 序列建模演示
# ════════════════════════════════════════════════════════════
def demo_rnn_sequence():
    """演示 RNN 的记忆和序列处理能力"""
    np.random.seed(42)

    # 生成正弦波序列
    t = np.linspace(0, 20 * np.pi, 200)
    x = np.sin(t).reshape(-1, 1)
    # 构建输入/输出对：用前 5 步预测下一步
    seq_len = 5
    X_seq, y_seq = [], []
    for i in range(len(x) - seq_len):
        X_seq.append(x[i:i+seq_len])
        y_seq.append(x[i+seq_len])
    X_seq = np.array(X_seq)
    y_seq = np.array(y_seq)

    # 简单 RNN
    rnn = SimpleRNN(input_size=1, hidden_size=16)
    outputs, final_h = rnn.forward(X_seq[0])

    print("  序列建模演示（正弦波预测）")
    print(f"  输入序列长度: {seq_len}")
    print(f"  总样本数: {len(X_seq)}")
    print(f"  输入形状: {X_seq[0].shape}")

    # RNN 的记忆能力
    print(f"\n  简单 RNN 前向传播")
    print(f"  输入序列: {[f'{v[0]:.3f}' for v in X_seq[0][:5]]}")
    print(f"  隐藏状态序列（最后 3 步）:")
    for t in range(2, 5):
        print(f"    t={t+1}: h = [{outputs[t, 0]:.3f}, {outputs[t, 1]:.3f}, ...]")
    print(f"  目标值: {y_seq[0][0]:.3f}")
    print()


# ════════════════════════════════════════════════════════════
# 5. RNN vs LSTM vs GRU 对比
# ════════════════════════════════════════════════════════════
def demo_rnn_variants():
    """对比 RNN / LSTM / GRU 的结构差异"""
    np.random.seed(42)
    input_size = 4
    hidden_size = 8
    seq_len = 3
    x_seq = np.random.randn(seq_len, input_size)

    print("  RNN / LSTM / GRU 结构对比")

    # Simple RNN
    rnn = SimpleRNN(input_size, hidden_size)
    rnn_out, rnn_h = rnn.forward(x_seq)
    print(f"  简单 RNN: 输入 {input_size} → 隐藏 {hidden_size}")
    print(f"    参数计数: W_xh({input_size}×{hidden_size}) + W_hh({hidden_size}×{hidden_size}) + b = "
          f"{input_size*hidden_size + hidden_size*hidden_size + hidden_size}")
    print(f"    最后隐藏状态 max: {np.abs(rnn_h).max():.4f}（梯度可能衰减）")

    # LSTM
    lstm = LSTMCell(input_size, hidden_size)
    lstm_out, (h, c) = lstm_forward(lstm, x_seq)
    print(f"\n  LSTM: 输入 {input_size} → 隐藏 {hidden_size} + 细胞状态 {hidden_size}")
    print(f"    参数计数: 4×({input_size}×{hidden_size} + {hidden_size}×{hidden_size} + bias) = "
          f"{4*(input_size*hidden_size + hidden_size*hidden_size + hidden_size)}")
    print(f"    细胞状态 max: {np.abs(c).max():.4f}（梯度通过门控保持，缓解梯度消失）")
    print(f"    隐藏状态 max: {np.abs(h).max():.4f}")

    # GRU
    gru = GRUCell(input_size, hidden_size)
    h_prev = np.zeros((1, hidden_size))
    for t in range(seq_len):
        h_prev = gru.forward(x_seq[t:t+1], h_prev)
    print(f"\n  GRU: 输入 {input_size} → 隐藏 {hidden_size}（无额外细胞状态）")
    print(f"    参数计数: 3×({input_size}×{hidden_size} + {hidden_size}×{hidden_size} + bias) = "
          f"{3*(input_size*hidden_size + hidden_size*hidden_size + hidden_size)}")
    print(f"    参数比 LSTM 少约 25%，但表达能力接近")
    print()


# ════════════════════════════════════════════════════════════
# 6. 门控机制可视化
# ════════════════════════════════════════════════════════════
def demo_gating_mechanism():
    """演示 LSTM 门控机制"""
    print("  LSTM 门控机制详解")
    print()
    print("  遗忘门 (forget gate):")
    print("    f_t = σ(W_f · [h_{t-1}, x_t] + b_f)")
    print("    控制哪些旧信息需要丢弃。输出 0~1，1 表示完全保留")
    print()
    print("  输入门 (input gate):")
    print("    i_t = σ(W_i · [h_{t-1}, x_t] + b_i)")
    print("    Ĉ_t = tanh(W_C · [h_{t-1}, x_t] + b_C)")
    print("    控制哪些新信息需要存入细胞状态")
    print()
    print("  细胞状态更新:")
    print("    C_t = f_t * C_{t-1} + i_t * Ĉ_t")
    print("    遗忘旧信息 + 添加新信息 —— 这是 LSTM 的核心")
    print()
    print("  输出门 (output gate):")
    print("    o_t = σ(W_o · [h_{t-1}, x_t] + b_o)")
    print("    h_t = o_t * tanh(C_t)")
    print("    控制哪些细胞状态信息输出到隐藏状态")
    print()

    # 数值演示
    np.random.seed(42)
    dummy_x = np.random.randn(1, 4)
    dummy_h = np.random.randn(1, 8)
    _, _ = np.random.randn(1, 8)  # c_prev (not used in demo)

    print("  GRU 门控 (简化版):")
    print("    重置门 r_t: 控制遗忘历史多少")
    print("    更新门 z_t: 控制用新信息更新多少（类似 LSTM 的遗忘+输入合并）")
    print("    无单独的细胞状态，参数更少，训练更快")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  RNN — 序列建模/LSTM/GRU/时序预测      ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_rnn_sequence()
    demo_rnn_variants()
    demo_gating_mechanism()

    print("[[OK]] RNN 演示完成")
