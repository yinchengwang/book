# db_flamegraph — 数据库火焰图

## 简介

演示 `perf` + `FlameGraph` 工具组合生成 CPU 火焰图，可视化数据库查询执行路径的热点。

## 编译

```bash
gcc -std=c11 -Wall -o db_flamegraph main.c
```

## 运行与生成火焰图

```bash
# 1. 编译执行
make run

# 2. 用 perf 采样
perf record -F 99 -g -o perf.data ./db_flamegraph

# 3. 生成火焰图（需安装 FlameGraph）
git clone https://github.com/brendangregg/FlameGraph.git
perf script -i perf.data | FlameGraph/stackcollapse-perf.pl | \
    FlameGraph/flamegraph.pl --title="DB Query" > db_flame.svg
```

## 火焰图阅读

- **X 轴**：总时间占比，宽 = 多
- **Y 轴**：调用栈深度，深 = 调用链长
- **颜色**：随机，帮助区分栈块
- **热点**：平坦的宽顶部 = CPU 热点

## 扩展

- 生成冷火焰图分析 I/O 等待
- 使用差分火焰图对比优化前后
