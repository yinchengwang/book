# S8 Spec —— Vector Delete Test gtest Discover 修复

## 1. 工程层 algo-prod 库契约

`engineering/src/algo-prod/` 只承载**库代码**，不得包含：
- ❌ `int main()` （独立 demo main）
- ❌ LeetCode 风格的含 main() 的题解文件
- ❌ 学习性质的 `binary_search/problems/`

允许：
- ✅ K-Means、距离、PQ/SQ/LVQ 量化
- ✅ 二分查找基础库（不含 main 的版本）
- ✅ Hash、Map、List、Queue、Stack、Priority Queue（基础数据结构）

## 2. 学习层 algo-c/problems/ 契约

`learning/algo-c/problems/` 容纳所有 LeetCode 风格的题解（包括 demo main）：
- ✅ `learning/algo-c/problems/leetcode_2070/leetcode_2070.c`
- ✅ `learning/algo-c/problems/leetcode_3280/leetcode_3280.c`

## 3. vector_delete_test gtest 集成

`engineering/test/db/index/vector_index/delete/vector_delete_test` 必须：
- ✅ 通过 `gtest_main` 注册到 ctest
- ✅ `gtest_discover_tests` 应发现 4+ 个 TEST_F
- ✅ 不依赖任何含 `int main()` 的工程库

## 4. 排除过滤器

`engineering/src/algo-prod/CMakeLists.txt` 排除规则：

```cmake
list(FILTER ALGO_PROD_SOURCE_FILE EXCLUDE REGEX "^dict/")           # S6: 学习性词典
list(FILTER ALGO_PROD_SOURCE_FILE EXCLUDE REGEX "^distributed/impl/") # S6: 学习性分布式
list(FILTER ALGO_PROD_SOURCE_FILE EXCLUDE REGEX "^binary_search/problems/") # S8: 学习性 LeetCode 含 main
```

## 5. 不做（明确范围外）

- ❌ 重写 leetcode_2070 / leetcode_3280 实现
- ❌ 强制 vector_index 100% 测试通过（个别测试需运行时数据）
- ❌ 处理 `db/cli/cli_main.c` 与 `db_server.c`（不同 main 范畴）
