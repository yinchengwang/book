"""
cnn — 卷积神经网络

演示卷积操作、池化、图像特征提取和简单分类。
纯 numpy 实现，无外部深度学习框架依赖。
"""

import numpy as np


# ════════════════════════════════════════════════════════════
# 1. 卷积操作
# ════════════════════════════════════════════════════════════
def conv2d(image, kernel, stride=1, padding=0):
    """2D 卷积操作（单通道）"""
    h, w = image.shape
    k = kernel.shape[0]
    # 填充
    if padding > 0:
        image = np.pad(image, padding, mode="constant")
        h, w = image.shape
    # 输出尺寸
    out_h = (h - k) // stride + 1
    out_w = (w - k) // stride + 1
    output = np.zeros((out_h, out_w))
    for i in range(out_h):
        for j in range(out_w):
            output[i, j] = np.sum(image[i*stride:i*stride+k, j*stride:j*stride+k] * kernel)
    return output


def conv2d_multi_channel(image, kernels, bias=None, stride=1, padding=0):
    """多通道 2D 卷积"""
    c_in, h, w = image.shape
    c_out, _, k, _ = kernels.shape
    out_h = (h + 2*padding - k) // stride + 1
    out_w = (w + 2*padding - k) // stride + 1
    output = np.zeros((c_out, out_h, out_w))
    for co in range(c_out):
        for ci in range(c_in):
            output[co] += conv2d(image[ci], kernels[co, ci], stride, padding)
        if bias is not None:
            output[co] += bias[co]
    return output


# ════════════════════════════════════════════════════════════
# 2. 池化操作
# ════════════════════════════════════════════════════════════
def max_pool2d(image, pool_size=2, stride=2):
    """最大池化"""
    h, w = image.shape
    out_h = (h - pool_size) // stride + 1
    out_w = (w - pool_size) // stride + 1
    output = np.zeros((out_h, out_w))
    for i in range(out_h):
        for j in range(out_w):
            output[i, j] = np.max(image[i*stride:i*stride+pool_size, j*stride:j*stride+pool_size])
    return output


def avg_pool2d(image, pool_size=2, stride=2):
    """平均池化"""
    h, w = image.shape
    out_h = (h - pool_size) // stride + 1
    out_w = (w - pool_size) // stride + 1
    output = np.zeros((out_h, out_w))
    for i in range(out_h):
        for j in range(out_w):
            output[i, j] = np.mean(image[i*stride:i*stride+pool_size, j*stride:j*stride+pool_size])
    return output


# ════════════════════════════════════════════════════════════
# 3. 卷积核演示
# ════════════════════════════════════════════════════════════
def demo_kernels():
    """演示不同卷积核对图像特征提取的效果"""
    # 生成一个简单的人工"图像"——包含边缘和纹理
    img = np.zeros((32, 32))
    img[8:24, 8:24] = 1.0  # 中心方块
    img[14:18, 14:18] = 0.0  # 中心小孔
    # 添加渐变边缘
    for i in range(5):
        img[8+i, 8:24] = 1.0 - i * 0.1
        img[24-i-1, 8:24] = 1.0 - i * 0.1

    # 常见卷积核
    kernels = {
        "水平边缘检测": np.array([[-1, -1, -1], [0, 0, 0], [1, 1, 1]]),
        "垂直边缘检测": np.array([[-1, 0, 1], [-1, 0, 1], [-1, 0, 1]]),
        "锐化": np.array([[0, -1, 0], [-1, 5, -1], [0, -1, 0]]),
        "高斯模糊": np.array([[1, 2, 1], [2, 4, 2], [1, 2, 1]]) / 16,
        "浮雕": np.array([[-2, -1, 0], [-1, 1, 1], [0, 1, 2]]),
    }

    print("  卷积核演示（32×32 人工图像）")
    for name, kernel in kernels.items():
        output = conv2d(img, kernel)
        print(f"  {name}:")
        print(f"    核形状: {kernel.shape}")
        print(f"    输出形状: {output.shape}")
        print(f"    响应范围: [{output.min():.3f}, {output.max():.3f}]")
        if np.abs(output).max() > 0:
            print(f"    最大响应位置（特征激活）：({np.unravel_index(np.argmax(np.abs(output)), output.shape)[0]}, {np.unravel_index(np.argmax(np.abs(output)), output.shape)[1]})")
        print()


# ════════════════════════════════════════════════════════════
# 4. 池化演示
# ════════════════════════════════════════════════════════════
def demo_pooling():
    """演示池化操作的效果"""
    np.random.seed(42)
    # 小图像
    img = np.random.randn(8, 8) + 0.5

    max_pooled = max_pool2d(img, pool_size=2, stride=2)
    avg_pooled = avg_pool2d(img, pool_size=2, stride=2)

    print("  池化操作对比")
    print(f"  输入形状: {img.shape}")
    print(f"  最大池化输出: {max_pooled.shape}")
    print(f"  平均池化输出: {avg_pooled.shape}")
    print(f"  最大池化保留了显著特征（取每个窗口最大值）")
    print(f"  平均池化平滑了特征（取每个窗口均值），对噪声更鲁棒")
    print()

    # 池化的平移不变性演示
    img1 = np.zeros((10, 10))
    img1[3:5, 3:5] = 1.0
    img2 = np.zeros((10, 10))
    img2[5:7, 5:7] = 1.0  # 平移

    pool1 = max_pool2d(img1, 2, 2)
    pool2 = max_pool2d(img2, 2, 2)

    print("  池化的近似平移不变性")
    print(f"  原图特征位置: (3,3), 池化后响应位置: ({np.where(pool1 > 0)[0][0]}, {np.where(pool1 > 0)[1][0]})")
    print(f"  平移后特征位置: (5,5), 池化后响应位置: ({np.where(pool2 > 0)[0][0]}, {np.where(pool2 > 0)[1][0]})")
    print(f"  池化使平移响应在输出空间变化更小\n")


# ════════════════════════════════════════════════════════════
# 5. 简单 CNN 分类演示
# ════════════════════════════════════════════════════════════
def demo_cnn_architecture():
    """演示 CNN 的完整前向传播流程"""
    np.random.seed(42)
    batch = 1
    channels = 3
    h, w = 32, 32
    # 模拟 RGB 图像
    image = np.random.randn(channels, h, w)

    print("  CNN 前向传播流程")

    # Conv1: 3→8 通道，3×3 核
    conv1_kernels = np.random.randn(8, 3, 3, 3) * 0.1
    conv1_bias = np.zeros(8)
    conv1_out = conv2d_multi_channel(image, conv1_kernels, conv1_bias, stride=1, padding=1)
    print(f"  Conv1: 3×32×32 → 8×{conv1_out.shape[1]}×{conv1_out.shape[2]}")

    # ReLU
    conv1_act = np.maximum(conv1_out, 0)
    print(f"  ReLU1: 激活函数（非线性变换）")

    # Pool1: 2×2 max pooling
    pool1_out = np.array([max_pool2d(conv1_act[c], 2, 2) for c in range(8)])
    print(f"  Pool1: 2×2 max pool → 8×{pool1_out.shape[1]}×{pool1_out.shape[2]}")

    # Conv2: 8→16 通道
    conv2_kernels = np.random.randn(16, 8, 3, 3) * 0.1
    conv2_out = conv2d_multi_channel(pool1_out, conv2_kernels, stride=1, padding=1)
    print(f"  Conv2: 8×16×16 → 16×{conv2_out.shape[1]}×{conv2_out.shape[2]}")

    # Pool2:
    pool2_out = np.array([max_pool2d(np.maximum(conv2_out[c], 0), 2, 2) for c in range(16)])
    print(f"  Pool2: → 16×{pool2_out.shape[1]}×{pool2_out.shape[2]}")

    # Flatten + FC
    flat = pool2_out.reshape(1, -1)
    print(f"  Flatten: → {flat.shape[1]} 维向量")

    # 全连接层 → 10 类
    fc_w = np.random.randn(flat.shape[1], 10) * 0.01
    logits = flat @ fc_w
    probs = np.exp(logits - logits.max()) / np.sum(np.exp(logits - logits.max()), axis=1, keepdims=True)
    pred_class = np.argmax(probs, axis=1)[0]
    print(f"  FC: → 10 类，Logits 输出")
    print(f"  预测类别: {pred_class}，置信度: {probs[0, pred_class]:.3f}")
    print()


if __name__ == "__main__":
    print("╔══════════════════════════════════════════╗")
    print("║  CNN — 卷积/池化/图像特征/分类         ║")
    print("╚══════════════════════════════════════════╝\n")

    demo_kernels()
    demo_pooling()
    demo_cnn_architecture()

    print("[[OK]] CNN 演示完成")
