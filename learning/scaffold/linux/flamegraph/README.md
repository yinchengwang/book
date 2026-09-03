# FlameGraph 火焰图

使用 `perf` + `FlameGraph` 进行 CPU 热点分析。

## 火焰图解读

火焰图 (Flame Graph) 是一种可视化 CPU 性能分析结果的图表，通过它可以直观地看到程序在执行过程中各个函数的 CPU 占用情况。

### 基本概念

| 概念 | 说明 |
|------|------|
| **横向宽度** | 表示该函数消耗的 CPU 时间比例，越宽说明耗时越长 |
| **纵向堆叠** | 表示调用栈，从下往上看，最底层是入口函数 (main) |
| **叶子热点** | 最顶端的方块代表实际消耗 CPU 的叶子函数 |
| **颜色含义** | 暖色 (红/橙) = CPU/热路径；冷色 (蓝/绿) = IO/等待 |

### 如何阅读

1. **从下往上看**：从底部的主函数开始，沿着调用栈向上
2. **看方块宽度**：越宽的函数调用占用越多 CPU 时间
3. **找顶端热点**：最顶端的宽方块是真正的 CPU 热点
4. **理解颜色**：红色/橙色区域是需要优化的热点代码

### 示例解读

```
                    ____ do_math ___
                   |               |
    main ------ process_data ---- compute_heavy ---- do_math
    0%           5%              20%                   75%
```

在上面的火焰图中，`do_math` 函数占据了 75% 的宽度，是主要的 CPU 热点。

## 优化案例：发现双循环热点

### 场景描述

假设我们有如下调用栈火焰图：

```
                                         ____ calc_hash ____
                                        |                    |
                    ____ inner_loop ____|                 inner_loop
                   |                     |                    |
    main ------ process_data -------- compute_heavy -------- do_math
```

### 问题分析

1. **双层循环模式**：在火焰图上表现为 `compute_heavy` 下有两个并排的 `inner_loop` 分支
2. **识别方法**：
   - 两个相邻且宽度相近的分支
   - 从同一个父函数向下延伸
   - 每个分支都有自己的 `calc_hash` 热点

### 优化方案

```cpp
// 优化前：O(n²) 复杂度
for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
        result += calc_hash(data[i], data[j]);  // 热点
    }
}

// 优化后：缓存已计算的哈希值
std::unordered_map<Key, size_t> hash_cache;
for (int i = 0; i < n; ++i) {
    auto key = make_key(data[i]);
    if (hash_cache.find(key) == hash_cache.end()) {
        hash_cache[key] = calc_hash(data[i], data[i]);
    }
    for (int j = 0; j < n; ++j) {
        result += hash_cache[key];  // 使用缓存
    }
}
```

## Windows 说明

> **注意**：`perf` 和 `FlameGraph` 是 Linux 特有的工具，需要在 Linux 或 WSL 环境中运行。

### Windows 用户选项

| 方案 | 说明 | 适用场景 |
|------|------|----------|
| **WSL 2** | Windows Subsystem for Linux 2 | 推荐的 Windows 开发方案 |
| **Docker Linux 容器** | 在容器中运行 Linux | 需要 Docker 环境 |
| **双系统** | 安装 Linux | 追求最佳兼容性 |

### WSL 2 使用步骤

1. **启用 WSL 2**：
   ```powershell
   wsl --install
   ```

2. **在 WSL 中安装工具**：
   ```bash
   sudo apt-get update
   sudo apt-get install linux-tools-common linux-tools-generic
   ```

3. **克隆 FlameGraph**：
   ```bash
   git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph
   ```

4. **运行脚本**：
   ```bash
   cd /mnt/c/code/book/learning/scaffold/linux/flamegraph
   ./generate_flamegraph.sh
   ```

## 使用方法

### 前提条件

- Linux 或 WSL 2 环境
- 安装了 `perf` 工具
- 安装了 `FlameGraph` (可选，但推荐)

### 快速开始

1. **安装 FlameGraph**：
   ```bash
   git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph
   ```

2. **运行脚本**：
   ```bash
   ./generate_flamegraph.sh
   ```

3. **查看火焰图**：
   - 用浏览器打开生成的 `flamegraph.svg`
   - 或启动本地服务器：
     ```bash
     python3 -m http.server 8000
     ```
     然后访问 http://localhost:8000/flamegraph.svg

### 手动运行

如果不想使用脚本，可以手动执行：

```bash
# 编译
g++ -O2 -g -o main main.cpp -lm

# perf 采样
perf record -F 99 -g --call-graph dwarf ./main

# 生成火焰图
perf script | ~/FlameGraph/stackcollapse-perf.pl | ~/FlameGraph/flamegraph.pl > flamegraph.svg
```

## perf 参数说明

| 参数 | 说明 |
|------|------|
| `-F 99` | 采样频率 99Hz (每秒采样 99 次) |
| `-g` | 收集调用栈信息 |
| `--call-graph dwarf` | 使用 DWARF 信息进行调用图回溯 |
| `-o perf.data` | 输出文件 (默认) |

## 参考资料

- [FlameGraph 官方仓库](https://github.com/brendangregg/FlameGraph)
- [perf 文档](https://www.man7.org/linux/man-pages/man1/perf.1.html)
- [Brendan Gregg 的火焰图博客](http://www.brendangregg.com/flamegraphs.html)
