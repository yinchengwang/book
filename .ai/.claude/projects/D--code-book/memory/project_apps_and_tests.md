---
name: project-apps-and-tests
description: project/ 独立玩具程序（贪吃蛇、2048、数独）和 test/ 测试体系的全貌
metadata: 
  node_type: memory
  type: project
  originSessionId: 79dcc910-7e0b-4040-aee9-56fe473e04bb
---

# 独立项目 & 测试体系

**Why:** 记录了 project/ 下的 4 个独立应用和 test/ 下的完整测试体系。

## project/ — 独立玩具程序

### 贪吃蛇 (Snake) — C11
- 100×50 矩形区域，三档难度（180/120/80ms）
- 动态速度：每吃 5 分升一级，按住方向键加速（速度临时减半）
- 核心算法：数组模拟链表、增量 ANSI 渲染、方向防反转
- 跨平台：Windows + POSIX

### 2048 — C11
- 4×4 棋盘，三档难度（初始块数 1/2/3）
- 核心算法：`slide_row` 三步法（压缩→合并→压缩）、矩阵变换（旋转+翻转）实现四方向移动
- 增量渲染：仅重绘变化格子
- 颜色映射 + 胜负判定 + 继续游戏

### 数独 (Sudoku) — C++17
- 9×9 标准数独，三档难度（30/40/50 空格）
- DFS 回溯求解器 + 计数求解器（唯一解验证）
- Fisher-Yates 洗牌 + 挖洞法动态生成题目
- 冲突实时检测、给定格保护、ANSI 颜色渲染

### 计算器 (Calculator) — 空骨架
- 所有源文件为空，根 CMakeLists 已注释此项目

## test/ — 测试体系

### 统计
- 测试源文件：40 个 (.cpp)
- CMakeLists.txt：22 个
- 框架：GoogleTest (gtest)
- 覆盖率：6 大模块全覆盖

### 按模块测试量

| 模块 | 测试文件数 | 测试用例(约) | 覆盖内容 |
|------|-----------|-------------|---------|
| algo/ | 12 | ~55 | binary_search(5)、dict(9)、distance(5)、kmeans(2)、list(4)、map(5)、queue(5)、sort(5)、stack(4) |
| self_made/ | 5 | ~29 | 链表(15)、队列(2)、字符串(4)、树(8) |
| self_made_cpp/ | 1 | 1 | Trie |
| leetcode/ | 11 | ~100 | C 版 10 个文件按题号范围测试，C++ 版 5 个文件 |
| interview/ | 1 | 1 | 华为OD面试题 |
| vector_index/ | 8 | ~80 | HNSW(8)、IVF(16)、DiskANN(24)、BM25(25)、CCEH(10)、B-tree(4)、B+tree(4)、tree_page(3) |

### 测试特点
- `all_in_one_test`：将 self_made + self_made_cpp + leetcode + interview 聚合到一个二进制
- 向量索引测试各模块独立编译（因依赖 algo 库）
- CCEH 有并发测试（多线程读写、epoch 回收）
- DiskANN 有堆单元测试（push/pop/heapify/skip）
- BM25 有 TAAT/DAAT 一致性交叉验证
- 所有持久化模块都有 save/load 往返测试
