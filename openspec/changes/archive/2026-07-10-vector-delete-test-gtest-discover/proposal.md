# S8 — Vector Delete Test gtest Discover 修复

## What Changes

工程层 `engineering/test/db/index/vector_index/delete/vector_delete_test.exe` 构建成功，但是运行只输出：

```
Result: 2 4 5 5 6 6
```

不接受任何 gtest 参数（`--gtest_list_tests` 没响应）。S7 续做时发现 vector_delete 的 ctest 注册里 `gtest_discover_tests` 返回**空测试列表**。

**根因**：algo-prod 库里有 2 个 .c 文件含独立 `int main()`：

```
engineering/src/algo-prod/binary_search/problems/2070_magic_list/leetcode_2070.c
    int main() {
        ...
        printf("Result: 2 4 5 5 6 6\n");  //  ← 这正是 vector_delete_test.exe 的输出
        ...
    }

engineering/src/algo-prod/binary_search/problems/3280_count_days/leetcode_3280.c
    int main() { ... }
```

这两个 .c 文件是**独立 LeetCode 题解 demo**，含自己的 main()，但 S4 algo-split 时它们被错留在 algo-prod 工程库，没迁到 learning/algo-c。当 vector_delete_test 链 algo-prod 时，多个 `main()` 冲突，链接器选择其中一个（这里是 leetcode_2070 的），导致 gtest_main 链路失效，gtest_discover_tests 返回 0。

**变更内容**：

1. **排除**：`engineering/src/algo-prod/CMakeLists.txt` 增加 `list(FILTER EXCLUDE REGEX "^binary_search/problems/")` —— 排除所有 problem .c 文件
2. **迁移**：
   - `engineering/src/algo-prod/binary_search/problems/2070_magic_list/leetcode_2070.c` → `learning/algo-c/problems/leetcode_2070/leetcode_2070.c`
   - `engineering/src/algo-prod/binary_search/problems/3280_count_days/leetcode_3280.c` → `learning/algo-c/problems/leetcode_3280/leetcode_3280.c`
3. **验证**：
   - V1: algo-prod 不再含 main()
   - V2: vector_delete_test gtest_discover_tests 返回真实测试数
   - V3: ctest -L vector_index 含 delete 子测试

## Why

**符合 CLAUDE.md 双轨铁律**：

- 学习性 LeetCode 答案（含独立 main demo）应归 learning/algo-c/problems/
- 工程层 algo-prod 应只放库函数（不含 main）

**α 价值（工程作品集）**：
- vector_delete 0 测试覆盖是 S7 留下的债务 —— 工程层核心数据路径（删除位图 + GC 真空 + 重建策略）应该有 4+ 个测试可跑
- 修复后 vector_index 测试覆盖从 10/10 (reranker only) → 26+ (reranker + delete)

**β 价值（学习日志）**：
- 两个 LeetCode demo 2070/3280 移到学习层更纯粹（双轨纪律一致）

**前置依赖**：
- S7 已让工程层全部构建
- `learning/algo-c/problems/` 目录已建立（7 个 leetcode 题解目录）

## Scope

**包含**：
- `engineering/src/algo-prod/CMakeLists.txt`：增加 problem 排除过滤器
- git mv 两个 .c 到学习层
- 验证学习层 cmake 仍能找到这两个文件
- 验证工程层 vector_delete_test gtest 集成

**不包含**：
- ❌ 重写 leetcode_2070 / leetcode_3280 实现（仅迁移位置）
- ❌ 给 vector_delete_test 添加新测试
- ❌ 解开 `learning/code-solutions/distributed/` 注释（phase9 待续做）

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| 学习层 algo-c/problems/ 其他位置已有 leetcode_2070 重复 | 低 | git mv 会冲突；先 grep 学习层确认 |
| 排除 problem/ 后破坏已有的其他 algo-prod test | 中 | V1-V3 验证工程层测试仍全通过 |
| `int main()` 在 binary_search/problems/ 子目录里未覆盖全部 | 低 | grep 全工程层确认就这 2 个 |
| CTestTestfile.cmake 含 vector_delete_test 但 gtest_main 还是被覆盖 | 中 | 算法最初 main 链中可能还有其他二进制 target |
