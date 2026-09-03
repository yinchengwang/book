# leetcode

LeetCode 题解，按题号范围分文件。

## 文件结构

- `c/` — C 语言实现，文件按题号范围命名（如 `leetcode_1_100.c`、`leetcode_2000_2100.c`）
- `cpp/` — C++ 实现（子目录）

GLOB_RECURSE 收集 `c/*.c` 和 `cpp/*.cpp`，输出到 `build/lib/libleetcode.a`。

## 依赖

链接 `project_includes`。

## 添加新题

在对应题号范围的文件中追加，或创建新的范围文件。文件名格式：`leetcode_<start>_<end>.c`（或 `.cpp`）。