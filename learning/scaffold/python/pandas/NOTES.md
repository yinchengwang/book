# pandas 学习笔记

## 概念地图

Pandas 是 Python 数据分析的核心库，基于 NumPy：

- **DataFrame**：表格数据，行/列的二维结构
- **Series**：一维序列，DataFrame 的列
- **groupby**：分组聚合，SQL 风格的 GROUP BY
- **join/merge**：表连接，SQL JOIN 在 Pandas 中的实现

## 踩坑记录

1. **链式赋值警告**：`df[df.a > 0]['b'] = 1` 可能不生效
2. **inplace 参数**：很多方法有 inplace 参数，推荐用赋值
3. **数据类型**：object 类型的列可能包含混合类型

## 工程对照（≥100 字硬约束）

### 1. 数据分析场景

Pandas 适合对项目的数据产出进行分析：

```python
import pandas as pd

# 分析测试结果数据
results = pd.read_csv('test-results/report.csv')
results.groupby('test_module')['passed'].mean()
```

### 2. 与 SQL 的对应关系

| SQL | Pandas |
|-----|--------|
| SELECT col1, col2 | df[['col1', 'col2']] |
| WHERE condition | df[df.col > 0] |
| GROUP BY | df.groupby() |
| ORDER BY | df.sort_values() |
| JOIN | pd.merge() |
| LIMIT | df.head() |

### 3. 数据读取

Pandas 支持多种格式读取，适合本项目的日志分析：

```python
pd.read_csv('log.csv')       # CSV
pd.read_excel('data.xlsx')   # Excel
pd.read_json('data.json')    # JSON
pd.read_sql(query, conn)     # SQL 数据库
```

学完本卡能动手的事：用 Pandas 处理 CSV/JSON 格式的数据集，进行筛选和聚合分析。
