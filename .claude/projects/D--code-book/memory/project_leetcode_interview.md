---
name: project-leetcode-interview
description: leetcode/ 解题代码库（~90+ 题）和 interview/ 面试知识库的结构与内容
metadata: 
  node_type: memory
  type: project
  originSessionId: 79dcc910-7e0b-4040-aee9-56fe473e04bb
---

# LeetCode 解题 & 面试知识库

**Why:** 记录了两个主要的学习目录：leetcode/（算法解题）和 Interview/（面试理论知识）。

## leetcode/ — 解题代码库

### 目录架构

三种组织方式共存：
1. **`src/leetcode/c/`** — 统一编译库，按题号范围分文件（如 `leetcode_600_700.c`），无 main()
2. **`src/leetcode/cpp/`** — C++ 版同样按范围分文件，使用 `LeetCode_Solution` 类
3. **`leetcode/`** — 每道题一个独立目录，含 main() + 测试用例，可直接编译运行

### 统计数据

- 已实现题目：约 90+ 道
- 源文件总数（含头文件）：约 61 个
- 使用的 C++ STL：vector, string, unordered_map/set, set, multiset, stack, priority_queue, algorithm, numeric
- 使用的第三方库：uthash

### 按主题分布

| 主题 | 代表性题目 |
|------|-----------|
| 数组 | 56(合并区间), 75(颜色排序), 414(第三大数), 581(最短无序子数组), 2016(最大差值) |
| 链表 | 19(删除倒数第N), 142(环形链表II), 143(重排链表), 206(反转链表), 445(两数相加II) |
| 树 | 100(相同树), 118(杨辉三角), 222(完全二叉树节点数), 1367(二叉树中的列表) |
| 字符串 | 1323(6和9最大数字), 2131(最长回文), 2284(最大单词数) |
| 栈 | 20(有效括号), 946(验证栈序列) |
| 哈希/计数 | 697(数组的度, 使用 uthash), 1338(数组大小减半), 1366(投票排名) |
| 贪心/堆 | 605(种花问题), 1705(吃苹果最大数, 最小堆), 3066(最少操作数II) |
| 数学/数论 | 231(2的幂), 326(3的幂), 7(整数反转), 2469(温度转换) |
| 设计题 | 2034(StockPrice类), 729(MyCalendar) |
| 面试题 | 17.14(最小K个数), 17.10(主要元素) |
| 竞赛 | 周赛433/436、双周赛148 共 5 题 |

## interview/ — 面试题源文件

- `src/interview/huawei/` — 华为面试题（最长恒温窗口 + Levenshtein编辑距离），C 和 C++ 双实现
- `src/interview/huawei_od/` — 华为OD面试题（补种胡杨树滑窗 + 环形字符串偶数字统计）
- C 版手写 Deque 数据结构，C++ 版使用 `std::deque`

## Interview/ — 面试理论知识库

| 文件 | 主题 | 规模 |
|------|------|------|
| `Interview/c_cpp/questions.md` | C/C++ 语言基础——49 道经典面试题 | 详细 |
| `Interview/database/SQL.md` | 数据库——35 道 MySQL/SQL 面试题 | 详细 |
| `Interview/VDB/vdb.md` | 向量数据库/RAG——索引原理、HNSW/IVF/PQ/DiskANN、RAG 全流程、文档切割、Re-rank、Embedding 演进、多路召回、评估、幻觉 | 最详细（约 800 行） |
| `Interview/c_cpp/leftover_problem.md` | 空占位 | — |
| `Interview/database/questions.md` | 空占位 | — |
