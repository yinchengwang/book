#!/usr/bin/env python3
# =============================================================================
# pandas 演示脚本
# 演示 DataFrame + 读取/筛选/聚合
# =============================================================================

import sys

try:
    import pandas as pd
    import numpy as np
except ImportError:
    print("错误：需要安装 pandas 和 numpy")
    print("运行: pip install pandas numpy")
    sys.exit(1)

# =============================================================================
# 1. DataFrame 创建
# =============================================================================
def demo_dataframe_create():
    """演示 DataFrame 创建"""
    print("\n" + "=" * 60)
    print("1. DataFrame 创建")
    print("=" * 60)

    # 从字典创建
    df = pd.DataFrame({
        '姓名': ['张三', '李四', '王五', '赵六'],
        '年龄': [25, 30, 35, 28],
        '城市': ['北京', '上海', '广州', '深圳'],
        '工资': [15000, 18000, 22000, 16000]
    })
    print("从字典创建 DataFrame:")
    print(df)

    # 从 NumPy 数组创建
    df_np = pd.DataFrame(
        np.random.randn(4, 3),
        columns=['A', 'B', 'C'],
        index=['row1', 'row2', 'row3', 'row4']
    )
    print(f"\n从 NumPy 数组创建:\n{df_np}")

    print("✓ DataFrame 创建演示完成")

# =============================================================================
# 2. 数据读取
# =============================================================================
def demo_data_reading():
    """演示数据读取（模拟文件操作）"""
    print("\n" + "=" * 60)
    print("2. 数据读取（模拟）")
    print("=" * 60)

    # 用字典模拟 CSV
    data = {
        'date': pd.date_range('2024-01-01', periods=5, freq='D'),
        'sales': [100, 200, 150, 300, 250],
        'cost': [60, 120, 90, 180, 150],
    }
    df = pd.DataFrame(data)
    print("模拟销售数据:")
    print(df)

    # 基本信息
    print(f"\n基本信息:")
    print(f"  行数: {len(df)}")
    print(f"  列数: {len(df.columns)}")
    print(f"  列名: {list(df.columns)}")

    # 数据类型
    print(f"\ndtypes:")
    print(df.dtypes)

    # 常用函数
    print(f"\nhead(3):\n{df.head(3)}")
    print(f"\ndescribe():\n{df.describe()}")

    print("✓ 数据读取演示完成")

# =============================================================================
# 3. 数据筛选
# =============================================================================
def demo_filtering():
    """演示数据筛选"""
    print("\n" + "=" * 60)
    print("3. 数据筛选与切片")
    print("=" * 60)

    # 创建数据
    df = pd.DataFrame({
        '姓名': ['张三', '李四', '王五', '赵六', '孙七'],
        '部门': ['技术', '市场', '技术', '运营', '市场'],
        '工资': [15000, 18000, 22000, 16000, 19000],
        '入职年': [3, 5, 7, 2, 4]
    })

    print("原始数据:")
    print(df)

    # 条件筛选
    print(f"\n工资 > 17000:\n{df[df['工资'] > 17000]}")

    # 多条件
    print(f"\n技术部门且工资 > 16000:\n{df[(df['部门'] == '技术') & (df['工资'] > 16000)]}")

    # 列选择
    print(f"\n仅姓名和工资:\n{df[['姓名', '工资']]}")

    # loc / iloc
    print(f"\nloc 第一行:\n{df.loc[0]}")
    print(f"\niloc 前两行:\n{df.iloc[:2]}")
    print(f"\nloc 姓名='张三'的行:\n{df.loc[df['姓名'] == '张三']}")

    print("✓ 数据筛选演示完成")

# =============================================================================
# 4. 数据聚合
# =============================================================================
def demo_aggregation():
    """演示数据聚合"""
    print("\n" + "=" * 60)
    print("4. 数据聚合（groupby）")
    print("=" * 60)

    df = pd.DataFrame({
        '部门': ['技术', '市场', '技术', '运营', '市场', '技术'],
        '员工': ['张三', '李四', '王五', '赵六', '孙七', '周八'],
        '工资': [15000, 18000, 22000, 16000, 19000, 17000],
        '绩效': [85, 90, 95, 78, 88, 92]
    })

    print("原始数据:")
    print(df)

    # groupby 聚合
    print(f"\n按部门统计:")
    grouped = df.groupby('部门').agg({
        '工资': ['mean', 'max', 'min', 'count'],
        '绩效': 'mean'
    })
    print(grouped)

    # 简单聚合
    print(f"\n各部门平均工资:")
    print(df.groupby('部门')['工资'].mean())

    # 排序
    print(f"\n按工资降序:\n{df.sort_values('工资', ascending=False)}")

    # 列操作：计算新列
    df['年薪'] = df['工资'] * 12 * (df['绩效'] / 100)
    print(f"\n添加年薪列:\n{df[['员工', '部门', '工资', '年薪']]}")

    print("✓ 数据聚合演示完成")

# =============================================================================
# 5. 数据清洗
# =============================================================================
def demo_cleaning():
    """演示数据清洗"""
    print("\n" + "=" * 60)
    print("5. 数据清洗基础")
    print("=" * 60)

    # 创建包含空值的数据
    df = pd.DataFrame({
        'A': [1, 2, None, 4, 5],
        'B': [None, 2, 3, None, 5],
        'C': ['x', 'y', 'z', None, 'w']
    })
    print("包含空值的数据:")
    print(df)

    # 检查空值
    print(f"\n检查空值:\n{df.isnull()}")
    print(f"\n每列空值数:\n{df.isnull().sum()}")

    # 填充空值
    print(f"\n填充空值 (fillna 0):\n{df.fillna(0)}")
    print(f"\n前向填充 (ffill):\n{df.fillna(method='ffill')}")

    # 删除空值
    print(f"\n删除含空值的行:\n{df.dropna()}")

    print("✓ 数据清洗演示完成")

# =============================================================================
# 6. 数据导出
# =============================================================================
def demo_export():
    """演示数据导出"""
    print("\n" + "=" * 60)
    print("6. 数据导出")
    print("=" * 60)

    df = pd.DataFrame({
        'name': ['Alice', 'Bob', 'Charlie'],
        'age': [25, 30, 35],
        'score': [85.5, 92.3, 78.8]
    })

    # CSV 导出
    print("导出为 CSV:")
    csv_out = df.to_csv(index=False)
    print(csv_out[:100])

    # JSON 导出
    print(f"导出为 JSON:\n{df.to_json(orient='records')[:100]}")

    # Excel 导出（需要 openpyxl）
    print("\nExcel 导出: df.to_excel('data.xlsx', index=False)")

    print("✓ 数据导出演示完成")

# =============================================================================
# 主函数
# =============================================================================
def main():
    print("\n" + "╔" + "=" * 58 + "╗")
    print("║" + " " * 15 + "Pandas 数据分析演示" + " " * 19 + "║")
    print("║" + " " * 8 + "DataFrame、筛选、聚合、清洗" + " " * 10 + "║")
    print("╚" + "=" * 58 + "╝")

    demo_dataframe_create()
    demo_data_reading()
    demo_filtering()
    demo_aggregation()
    demo_cleaning()
    demo_export()

    print("\n" + "=" * 60)
    print("演示完成！关键知识点：")
    print("=" * 60)
    print("• pd.DataFrame()           # 创建 DataFrame")
    print("• df[条件] / df.loc[]     # 数据筛选")
    print("• df.groupby().agg()      # 分组聚合")
    print("• df.isnull()/fillna()    # 空值处理")
    print("• df.to_csv/to_json       # 数据导出")
    print("=" * 60)

if __name__ == '__main__':
    main()
