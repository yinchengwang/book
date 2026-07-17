# control_flow 学习笔记

## 概念地图

C 语言的控制流语句分为三类：**选择**（if/switch）、**循环**（for/while/do-while）、**跳转**（break/continue/goto/return）：

- **if 链的"早 return"模式**：每条 `if` 处理一个错误条件后立刻 `return`，主逻辑只剩最浅的缩进。Linux 内核编码风格强制要求此模式
- **switch 的本质**：编译期跳转表（jump table）优化。如果 case 值密集（如 ASCII 字符），编译器生成 O(1) 跳转表；如果稀疏，退化为 if-else 链。**没有 break 就会 fallthrough 到下一个 case**
- **for 三段式语义**：`for (init; cond; step) body` ≡ `init; while (cond) { body; step; }`，但 for 把所有循环控制集中在头部，可读性更好
- **while vs do-while**：while 可能 0 次执行（先检查后执行），do-while 至少 1 次（先执行后检查）——do-while 常用于"必须执行一次"的菜单/交互逻辑
- **goto 的合法用法**：跳出多层嵌套循环 + 集中 cleanup。**绝对禁止**用 goto 跨越变量声明（会跳过初始化）。Linux 内核 `goto cleanup` 是业界典范

## 踩坑记录

1. **switch 中遗漏 break**：`case 1: case 2: ...` 是合法的"或"语义，但 `case 1: do_x(); case 2: do_y();` 会执行两个分支——除非有意为之，必须每个 case 末尾加 break 或注释
2. **for 循环里的浮点步进**：`for (double x = 0.0; x != 1.0; x += 0.1)` 永远结束不了——0.1 在 IEEE 754 中无法精确表示。改用 `x < 1.0` 比较
3. **无限循环的边界问题**：`while (1)` 或 `for (;;)` 是合法的，但**循环内必须有 break 或 return 出口**——否则 Ctrl+C 是唯一出路
4. **continue 跳过的不仅是本轮剩余代码**：for 循环里 continue 会执行 step（如 `i++`），但跳过 body
5. **goto 跨函数 = UB**：goto 只能在同一函数内跳转，不能跨函数（无 setjmp/longjmp 帮助）

## 工程对照（≥100 字硬约束）

控制流语句在本仓库 `engineering/` 中体现得淋漓尽致：

1. **多 case switch 集中在 `engineering/src/redis/redis_sds.c`**：行 17/58/86 三处 switch，分别处理 SDS 类型掩码、字符串创建模式、释放模式。Redis 是"小型 switch 表"哲学的极致体现——每个 case 都是一个独立分支
2. **嵌套 for 循环遍历元数据**：`engineering/src/db/view/view.c` 行 67-73 演示了双层 for 循环遍历视图管理器——外层遍历视图，内层遍历列；行 156 `for (int j = i; j < mgr->view_count - 1; j++)` 删除视图时的数组前移
3. **大循环扫描 SQL 表结构**：`engineering/src/db/validator/sql_semantic.c` 行 64 `for (size_t i = 0; i < stored_meta->num_columns; i++)` 扫描所有列，是 SQL 语义分析的热路径
4. **goto cleanup 模式**：`engineering/src/db/validator/constraint_checker.c` 行 377-404 的 switch-case 是大状态机——约束检查的错误处理通过 case 标签分发
5. **早 return 错误处理**：整个 `engineering/src/db/api/*.c` 都遵循"先检查参数，失败立刻 return"的模式，例如 `engineering/src/db/api/vector_api.c` 中所有 vector API 都先验证输入参数
6. **三层 case 合并**：`engineering/src/db/validator/constraint_checker.c` 行 404 `switch (error)` 处理 8 种错误码，是 switch 在生产代码中的标准用法

学完本卡能动手的事：扫描 `learning/scaffold/` 下所有 main.c，把任何 4 层以上嵌套的 `if` 改为"早 return"扁平化。