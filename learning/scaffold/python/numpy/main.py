#!/usr/bin/env python3
# =============================================================================
# numpy 演示脚本
# 演示 ndarray + 运算 + 矩阵操作
# =============================================================================

import sys

try:
    import numpy as np
except ImportError:
    print("错误：需要安装 numpy")
    print("运行: pip install numpy")
    sys.exit(1)

# =============================================================================
# 1. ndarray 创建与属性
# =============================================================================
def demo_ndarray():
    """演示 ndarray 创建与基本属性"""
    print("\n" + "=" * 60)
    print("1. ndarray 创建与属性")
    print("=" * 60)

    # 从 list 创建
    arr1 = np.array([1, 2, 3, 4, 5])
    print(f"从 list 创建: {arr1}")

    # 多维数组
    arr2d = np.array([[1, 2, 3], [4, 5, 6]])
    print(f"二维数组:\n{arr2d}")

    # 特殊数组
    print(f"\n全零数组: {np.zeros((2, 3))}")
    print(f"全1数组: {np.ones((2, 3))}")
    print(f"单位矩阵:\n{np.eye(3)}")

    # 数组属性
    print(f"\n维度: {arr2d.ndim}")
    print(f"形状: {arr2d.shape}")
    print(f"元素个数: {arr2d.size}")
    print(f"数据类型: {arr2d.dtype}")

    print("✓ ndarray 创建演示完成")

# =============================================================================
# 2. 数组运算
# =============================================================================
def demo_operations():
    """演示数组运算"""
    print("\n" + "=" * 60)
    print("2. 数组运算（向量化操作）")
    print("=" * 60)

    a = np.array([1, 2, 3, 4])
    b = np.array([5, 6, 7, 8])

    print(f"a = {a}")
    print(f"b = {b}")
    print(f"a + b = {a + b}")
    print(f"a * b = {a * b}")  # 逐元素乘法
    print(f"a ** 2 = {a ** 2}")
    print(f"a > 2 = {a > 2}")  # 布尔数组
    print(f"a[a > 2] = {a[a > 2]}")  # 布尔索引

    # 统计函数
    print(f"\nsum: {a.sum()}")
    print(f"mean: {a.mean():.2f}")
    print(f"max: {a.max()}")
    print(f"min: {a.min()}")

    print("✓ 数组运算演示完成")

# =============================================================================
# 3. 矩阵操作
# =============================================================================
def demo_matrix():
    """演示矩阵操作"""
    print("\n" + "=" * 60)
    print("3. 矩阵操作")
    print("=" * 60)

    # 矩阵乘法
    A = np.array([[1, 2], [3, 4]])
    B = np.array([[5, 6], [7, 8]])

    print(f"矩阵 A:\n{A}")
    print(f"矩阵 B:\n{B}")
    print(f"A @ B (矩阵乘法):\n{A @ B}")
    print(f"np.dot(A, B):\n{np.dot(A, B)}")

    # 转置和逆
    print(f"\nA 的转置:\n{A.T}")
    print(f"A 的逆:\n{np.linalg.inv(A)}")

    # 行列式和特征值
    print(f"det(A): {np.linalg.det(A):.2f}")
    print(f"特征值: {np.linalg.eigvals(A)}")

    print("✓ 矩阵操作演示完成")

# =============================================================================
# 4. 广播与切片
# =============================================================================
def demo_broadcasting():
    """演示广播机制和切片"""
    print("\n" + "=" * 60)
    print("4. 广播机制与切片")
    print("=" * 60)

    # 广播：不同形状的数组运算
    row = np.array([1, 2, 3])
    col = np.array([[4], [5], [6]])
    print(f"row + col:\n{row + col}")
    print(f"row * 3: {row * 3}")

    # 切片
    arr = np.arange(12).reshape(3, 4)
    print(f"\n数组 (3x4):\n{arr}")
    print(f"第一行: {arr[0]}")
    print(f"前两列:\n{arr[:, :2]}")
    print(f"第二列: {arr[:, 1]}")

    print("✓ 广播与切片演示完成")

# =============================================================================
# 5. 重塑与合并
# =============================================================================
def demo_reshape():
    """演示重塑、合并和分割"""
    print("\n" + "=" * 60)
    print("5. 重塑与合并")
    print("=" * 60)

    arr = np.arange(12)

    print(f"原始: {arr}")
    print(f"reshape(3,4):\n{arr.reshape(3, 4)}")
    print(f"ravel (展平): {arr.reshape(3, 4).ravel()}")

    a = np.array([1, 2, 3])
    b = np.array([4, 5, 6])
    print(f"np.hstack([a,b]): {np.hstack([a, b])}")
    print(f"np.vstack([a,b]):\n{np.vstack([a, b])}")

    print("✓ 重塑与合并演示完成")

# =============================================================================
# 主函数
# =============================================================================
def main():
    print("\n" + "╔" + "=" * 58 + "╗")
    print("║" + " " * 15 + "NumPy 科学计算演示" + " " * 19 + "║")
    print("║" + " " * 10 + "ndarray、运算、矩阵操作" + " " * 14 + "║")
    print("╚" + "=" * 58 + "╝")

    demo_ndarray()
    demo_operations()
    demo_matrix()
    demo_broadcasting()
    demo_reshape()

    print("\n" + "=" * 60)
    print("演示完成！关键知识点：")
    print("=" * 60)
    print("• np.array()            # 创建数组")
    print("• arr.shape/dtype      # 数组属性")
    print("• a + b / a * b        # 向量化运算")
    print("• A @ B / np.dot()     # 矩阵乘法")
    print("• arr.reshape()        # 重塑形状")
    print("• broadcasting         # 广播机制")
    print("=" * 60)

if __name__ == '__main__':
    main()
