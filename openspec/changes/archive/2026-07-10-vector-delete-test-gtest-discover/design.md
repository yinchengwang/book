# S8 — 设计文档（Vector Delete gtest Discover 修复）

## 1. 目标

让 `vector_delete_test.exe` 的 `gtest_main` 链路正常工作，使 ctest 能发现并运行 4 个核心测试（bitmap/filter/gc/rebuild）。

## 2. 根因 + 修复

```
vector_delete_test.exe 的 main() 链：
  gtest_main() (libgtest_main.a)  vs
  leetcode_2070.c 的 main() (libalgo-prod.a)

冲突！链接器在冲突 .obj 中选择其中一个 → 选择 leetcode_2070 的 main
结果：vector_delete_test.exe 跑的是 2070 demo，不跑 gtest
```

## 3. 实施步骤

### 3.1 排除 + 迁移 problem/

```bash
# 1. 排除 binary_search/problems/ 从 algo-prod
# 2. 移到 learning/algo-c/problems/
git mv engineering/src/algo-prod/binary_search/problems/2070_magic_list/leetcode_2070.c  \
       learning/algo-c/problems/leetcode_2070/leetcode_2070.c
git mv engineering/src/algo-prod/binary_search/problems/3280_count_days/leetcode_3280.c \
       learning/algo-c/problems/leetcode_3280/leetcode_3280.c
rmdir engineering/src/algo-prod/binary_search/problems/{2070_magic_list,3280_count_days}
```

### 3.2 修改 algo-prod/CMakeLists.txt

```cmake
# 当前
list(FILTER ALGO_PROD_SOURCE_FILE EXCLUDE REGEX "^dict/")
list(FILTER ALGO_PROD_SOURCE_FILE EXCLUDE REGEX "^distributed/impl/")

# 新增 S8 排除 problem/（学习性 LeetCode 含 main() demo）
list(FILTER ALGO_PROD_SOURCE_FILE EXCLUDE REGEX "^binary_search/problems/")
```

### 3.3 学习层路径

`learning/algo-c/CMakeLists.txt` 已用 `GLOB_RECURSE *.c`，自动 pick up 移动的 2 个 .c。

## 4. 验证 V1-V4

| V | 命令 | 期望 |
|---|---|---|
| V1 | `nm build/libalgo-prod.a \| grep " T main"` | 零结果 |
| V2 | `engineering/build/test/db/index/vector_index/delete/vector_delete_test.exe --gtest_list_tests` | 至少 4 个 TEST |
| V3 | `ctest --test-dir engineering/build -L vector_index --output-on-failure` | reranker + delete ≥14 测试 |
| V4 | `cmake --build build-learning` | 成功（含 leetcode_2070/3280 在 algo-c 库中） |

## 5. 失败回退

如果 V2 仍 0 测试：
- 检查 vector_delete_test/link.txt 把 libgtest_main 提到 algo-prod 之前
- 或在 test CMakeLists 把 GTest::gtest_main 放在最后一项

## 6. 不做

- ❌ 重写 2070/3280 实现
- ❌ 把现有的工程层其他 main() 串改造（cli_main / db_server 等是不同范畴）
- ❌ 强制 vector_index 全 ctest 100% 通过（个别测试可能依赖运行时配置）
