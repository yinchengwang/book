# 目录重组设计方案

## Context

### 背景

当前项目代码组织存在以下问题：

1. **LeetCode 题目组织混乱**：39 道题目平铺在单一 `leetcode/` 目录，命名不统一（`a_match`、`interview_17_10`、`leetcode_100`）
2. **数据结构实现分散**：`src/self_made/` 和 `src/algo/ds/` 存在大量重复的数据结构实现
3. **缺少分类索引**：无法按数据结构和算法类型快速定位相关代码
4. **缺乏文档**：只有代码，没有详细的理论说明和可视化文档

### 当前结构

```
src/
├── self_made/           # 数据结构（堆、链表、栈、队列、树、排序、字符串）
│   ├── heap/
│   ├── list/
│   ├── queue/
│   ├── sort/
│   ├── stack/
│   ├── str/
│   └── tree/
├── algo/
│   ├── ds/              # 另一个数据结构实现集合（大量重复）
│   ├── binary_search/
│   ├── sort/
│   └── ...
└── leetcode/            # 39 道题目（平铺）
```

### 约束

- 必须保持 CMake 构建系统正常工作
- 必须保留所有现有代码功能
- 迁移过程需要可回滚

## Goals / Non-Goals

**Goals:**
- 建立清晰的数据结构和算法分类体系
- 每个分类包含详细的 README.md 文档（含 Mermaid 可视化）
- LeetCode 题目按所属分类归入对应的 `problems/` 目录
- 实现文件（impl/）、题目（problems/）、文档（README.md）三位一体
- 更新 CMakeLists.txt 构建配置

**Non-Goals:**
- 不修改任何代码实现逻辑
- 不重新实现已有功能
- 不创建 OpenSpec 规格中未列出的新分类
- 不处理 `src/cpp/`、`src/db/`、`src/index/`、`src/redis/` 目录（保持现状）

## Decisions

### Decision 1: 顶层目录结构

**选择**：创建 `src/ds/` 目录存放所有数据结构实现

**理由**：
- `ds` (Data Structures) 是业界通用缩写
- 与 `algo` (Algorithms) 并列，结构清晰
- 避免与现有 `algo/ds/` 混淆

**备选方案**：
- `src/data_structures/`：名称过长
- `src/structures/`：不够明确
- `src/containers/`：偏向 C++ STL 术语

### Decision 2: README.md 命名规范

**选择**：每个子目录一个 README.md

**理由**：
- 每个数据结构和算法独立文档
- 便于导航和阅读
- 与实现文件位置一致

**备选方案**：
- 统一放在父目录的 README 中：内容过长，不便于阅读

### Decision 3: Mermaid 可视化规范

**选择**：使用 Mermaid 流程图展示操作过程

**理由**：
- Mermaid 是 Markdown 原生支持的图表语法
- GitHub、GitLab 等平台原生渲染
- 比外部图片更易于维护

**规范**：
- 插入操作：新增元素用绿色 `#9f9` 标识
- 删除操作：被删元素用红色 `#f96` 标识
- 修改操作：被改元素用黄色 `#ff9` 标识

### Decision 4: 文件命名

**选择**：按题号命名，如 `0019_remove_nth/`

**理由**：
- 4 位补零便于排序
- 题目名称便于理解
- 避免与目录名冲突

**备选方案**：
- 只用题号 `0019/`：不够直观
- 只用题目 `remove_nth/`：不便于查找

### Decision 5: LeetCode 题目归类

**选择**：按题目主要考察的数据结构/算法归类

**理由**：
- 一道题可能涉及多个知识点，选择主要考察点
- 便于按分类学习

**分类映射**：
| 分类 | 题目 |
|------|------|
| 数组 | 80, 1338, 1493, 1705, 1984, 2006, 2270, 2275, 2469, 2595, 3046, 3065 |
| 链表 | 19, 142, 143, 206, 445 |
| 栈 | 20 |
| 二叉树 | 100, 222 |
| 堆 | 1705, 3066 |
| 哈希表 | 1366, 1367 |
| 字符串 | 551, 1941, 2264, 3019 |
| 位操作 | 2275, 2595 |
| 二分查找 | 2070, 3066, 3280 |
| 滑动窗口 | 1984, 2270, 3297 |
| 前缀和 | 3159 |

### Decision 6: include 目录组织

**选择**：与 src 结构镜像，创建 `include/ds/` 和 `include/algo/`

**理由**：
- 头文件位置与实现文件位置对应
- 便于 CMake 配置

### Decision 7: test 目录组织

**选择**：与 src 结构镜像，创建 `test/ds/` 和 `test/algo/`

**理由**：
- 测试与实现对应
- 便于维护

## Risks / Trade-offs

### Risk 1: 迁移过程中构建失败

**风险**：文件路径变化导致 CMake 找不到源文件

**缓解**：
- 先创建新目录结构
- 逐步迁移文件
- 每步迁移后验证构建
- 保留旧目录作为备份，直到完全验证

### Risk 2: Git 历史丢失

**风险**：文件移动导致 Git blame 历史中断

**缓解**：
- 使用 `git mv` 而非直接移动
- 保持文件内容不变

### Risk 3: 迁移工作量较大

**风险**：涉及多个目录，迁移任务繁重

**缓解**：
- 按分类分批执行
- 每批完成后更新 tasks.md
- 设定明确的验收标准

## Migration Plan

### 阶段 1: 准备工作
1. 创建 `src/ds/` 目录结构
2. 创建 `include/ds/` 目录结构
3. 创建 `test/ds/` 目录结构
4. 创建所有规格文档（已完成）

### 阶段 2: 迁移数据结构实现
1. 迁移 `src/self_made/` → `src/ds/`
2. 迁移 `src/algo/ds/` → `src/ds/`
3. 更新对应头文件到 `include/ds/`
4. 迁移测试到 `test/ds/`

### 阶段 3: 重组算法目录
1. 重组 `src/algo/sorting/`
2. 重组 `src/algo/binary_search/`
3. 创建新的 `src/algo/sliding_window/` 等目录
4. 更新对应头文件到 `include/algo/`

### 阶段 4: 迁移 LeetCode 题目
1. 创建所有题目的 `problems/` 目录
2. 迁移每个题目到对应分类
3. 创建每个题目的 README.md

### 阶段 5: 更新构建系统
1. 更新 `src/CMakeLists.txt`
2. 更新各子目录 CMakeLists.txt
3. 验证构建通过
4. 删除旧的 `src/self_made/`、`src/algo/ds/`

### 阶段 6: 创建文档
1. 为每个 `src/ds/` 子目录创建 README.md
2. 为每个 `src/algo/` 子目录创建 README.md
3. 包含 Mermaid 可视化图表

## Open Questions

1. **比赛题目归类**：`a_match/` 中的比赛题目是否单独存放，还是按题目类型归入各分类？
   - 当前方案：保持 `src/algo/contest/` 目录

2. **多解法题目**：同一题可能有 C 和 C++ 两种实现，是否需要分目录？
   - 当前方案：同一题目的多种实现放在同一目录

3. **排序算法具体实现**：`src/algo/sort/` 中的冒泡/选择/插入等具体排序，是否需要细分为 `src/algo/sorting/` 的子目录？
   - 当前方案：统一放在 `impl/` 目录，通过文件名区分
