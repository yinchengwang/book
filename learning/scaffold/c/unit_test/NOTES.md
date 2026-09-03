# unit_test 学习笔记

## 概念地图

单元测试不是"写完代码再补的体力活"——它是**代码行为的可执行规格**。一个测试用例 = 一个断言"给定输入 X，函数应返回 Y"：

- **手工断言宏的原理**（20 行）：
  - `TEST(name)` → `static void test_##name(void)` —— 用宏拼接生成函数名，避免手写 19 个函数签名
  - `ASSERT_EQ(a, b)` → 值不等时打印文件名、行号、实际值/预期值、`test_fails++`、`return` 跳出
  - `RUN_TEST(name)` → 调用测试函数，前后保存/对比 `test_fails` 计数器判断 PASS/FAIL
  - 关键设计决策：失败 `return` 而不 `exit()`——保证一个用例的失败不影响后续用例执行
- **边界值测试（BVT）**：不是随机挑几个值测——而是系统性地覆盖**最小值、正常值、最大值、错误输入**。对 factorial：0（边界）、1（最小正常）、5（正常）、-1（错误）、12（溢出边界）
- **测试覆盖率**：`gcov` 统计每行代码是否被执行过——如果某行的覆盖率是 0，说明没有测试用例经过这个分支。80% 行覆盖是业界共识的"最低可接受线"
- **regression test（回归测试）**：修了一个 bug ↔ 加一个测试用例——防止同一个 bug 在未来的重构中再次出现

## 踩坑记录

1. **宏中的 `return` 不能退出 `main`**：`ASSERT_EQ` 失败后 `return` 只退出当前的 `test_xxx()` 函数——如果把它放在 `main` 函数里直接调用（而不是通过 `RUN_TEST`），行为不同。这也是为什么 `RUN_TEST` 不能直接展开为调用 + 断言
2. **`__LINE__` 在宏展开时的位置**：`ASSERT_EQ` 中的 `__LINE__` 展开为宏被使用处的行号——不是宏定义处。这让你能精确定位失败的断言在 test.c 的第几行
3. **多文件编译的参数顺序**：`gcc -o test_runner test.c mathlib.c`——包含 `main` 的文件（test.c）放前面，被链接的文件（mathlib.c）放后面。顺序不影响 gcc，但影响某些老式链接器的符号解析
4. **手工宏的局限性**：不支持 `ASSERT_FLOAT_EQ`（浮点数比较需要 epsilon）、不支持 fixture/setup/teardown（每个 TEST 独立），不支持参数化测试（同一逻辑不同输入）。这是 20 行代码换来的代价——中大型项目用 Unity 或 Google Test
5. **本机 MinGW 无 make**：直接 `gcc -o test_runner test.c mathlib.c && ./test_runner`

## 工程对照（≥100 字硬约束）

单元测试在本仓库中有明确可用武之地，但当前未被充分利用：

1. **`engineering/test/algo/` — 23 个空目录待填充**：backtrack / binary_search / dp / kmp / sort / graph / ... 每个目录都在等一个 `test.cpp`。本卡的 `TEST()` + `ASSERT_EQ` 模式可以直接填入这些目录——对 `sort.c` 写 5 个用例，验证三个 O(n²) 排序在 {正序, 逆序, 随机, 单元素, 空数组} 五种输入下的正确性
2. **`engineering/src/db/api/handlers.c` — REST API 的可测试性**：9 个 handler（add/get/delete/search 等）都有明确的输入/输出契约——`get_handler(url, params) → json_response`。但当前缺少测试 harness（需要一个 mock HTTP request 的构造器）。本卡的手工断言宏可直接复制为 handler 测试的基础
3. **`engineering/src/algo-prod/sort/sort.c` — 最适合作单元测试的纯函数**：排序没有外部依赖（不读磁盘、不发网络、不调 DB），所有输入/输出都是内存中的数组。测试用例 = {输入数组, 期望输出数组}，一行 ASSERT_EQ 验证一组输入。这是"单元测试 ROI 最高"的代码类型
4. **Google Test 模式**：本仓库的 `engineering/test/` 全部用 gtest（C++ 框架通过 `extern "C"` 测试 C 代码）。`engineering/test/algo/` 下每个目录如果有 `test_*.cpp`，就会在 `cmake --build build/engineering` 时自动编译并注册到 ctest。本卡的 `TEST()` / `ASSERT_EQ()` 宏是 gtest 的迷你版，理解它就能理解 gtest 的底层

学完本卡后能动手的事：给 `engineering/test/algo/sort/` 写第一个测试文件 `test_sort.cpp`，用 Google Test 验证 `sort.c` 的三种排序算法——搭好 harness 后，后续所有 algo-prod/ 模块都可以按照相同模式补测试。
