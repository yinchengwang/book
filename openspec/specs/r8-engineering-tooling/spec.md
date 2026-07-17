# r8-engineering-tooling Specification

## Purpose
TBD - created by archiving change r8-engineering-deepening. Update Purpose after archive.
## Requirements
### Requirement: 四要素审计

每张 R8 卡 MUST 满足四要素审计：从 todo 推到 done 时，必须在 `statuses.json` 和 `r8-progress.md` 中同时满足四个条件：

1. **commit SHA 引用**：r8-progress.md 行中存在 commit SHA（当前统一为 TBD-deferred，与 R5/R6/R7 一致）
2. **scaffold 产物**：`learning/scaffold/<card-id>/` 目录下存在 main.c + Makefile（工具类卡可含额外文件如 platform.h/c、test.c、CHEATSHEET.md）
3. **NOTES.md 工程对照**：`learning/scaffold/<card-id>/NOTES.md` 含"工程对照"段，中文内容 ≥100 字符，至少引用一个 `engineering/` 路径
4. **r8-progress.md 行**：每张卡在 `r8-progress.md` 中有一行记录，时间戳、路径与 statuses.json 同步

#### Scenario: 算法卡交付

- **WHEN** c-string-match 或 c-dp 的 scaffold + docs + notes + status 四步完成
- **THEN** statuses.json 中对应卡 ID 值为 "done"
- **AND** r8-progress.md 中对应行的时间戳为当日、commit SHA 为 TBD-deferred

#### Scenario: 工具卡交付

- **WHEN** c-git / c-static-analysis / c-unit-test / c-profiling / c-secure-coding / c-cross-platform 的 scaffold + docs + notes + status 四步完成
- **THEN** 同上，且 Makefile 含对应工具专属 target（如 `check` / `test` / `profile`）
- **AND** 不可用工具执行专属 target 时 graceful 降级（exit 0 + skip 消息）

### Requirement: 算法卡可观察性约束

R8 中的 2 张算法卡 MUST 延续 R7 的算法可观察性规约：

- **字符串匹配**：每种算法打印 `[tag]` 标记的匹配过程——KMP 的 lps 数组构建表、BM 的坏字符跳转距离、RK 的每轮滚动哈希值
- **动态规划**：每种 DP 范式打印完整 DP 表格——背包的递推决策过程（`max(dp[i-1][w], dp[i-1][w-wt[i]]+val[i])`）、LCS 的回溯路径方向
- **退出码**：执行无错误时 `return 0`，结尾打印 `=== PASS ===`

#### Scenario: 字符串匹配中间步骤

- **WHEN** 运行 `./stringmatch_demo`（或 `gcc -o test main.c && ./test`）
- **THEN** 标准输出包含 `[bf step]`、`[kmp]`、`[bm]`、`[rk window]` 四种 tag 标记的中间匹配状态
- **AND** 退出码为 0

#### Scenario: DP 表格打印

- **WHEN** 运行 `./dp_demo`（或等效命令）
- **THEN** 标准输出包含 knapsack 的 `dp[i][w]` 递推决策行、LCS 的完整 DP 表 + 回溯路径
- **AND** 退出码为 0

### Requirement: 工具卡 Graceful 降级

R8 的 6 张工具/方法论卡 MUST 在 Makefile 的专属 target 中实现 graceful 降级：当所需工具在本机不可用时，打印跳过信息并以 exit 0 退出，不报编译/链接错误。

降级检测模式（与 c-code-style 一致）：
```makefile
check-cppcheck:
	@command -v cppcheck >/dev/null 2>&1 && cppcheck --enable=all main.c || echo "[skip] cppcheck 不可用"
```

#### Scenario: cppcheck 不可用时降级

- **WHEN** 在 MinGW 环境下运行 `cd learning/scaffold/c-static-analysis && make check-cppcheck`
- **THEN** 输出 `[skip] cppcheck 不可用`
- **AND** 退出码为 0

#### Scenario: gprof 不可用时降级

- **WHEN** 在 MinGW 环境下运行 `cd learning/scaffold/c-profiling && make profile`
- **THEN** 编译至少通过 clock() 手动计时方案；gprof 不可用时打印 `[skip] gprof 不可用，使用 clock() 计时`
- **AND** 退出码为 0

### Requirement: r8-progress.md 独立性

R8 MUST 创建独立进度文件 `r8-progress.md`，不复用 R5/R6/R7 的 progress.md。R8 实施期间不修改 r5-progress.md / r6-progress.md / r7-progress.md。

#### Scenario: progress 文件互不干扰

- **WHEN** R8 执行 8 张卡的 status 更新
- **THEN** r8-progress.md 新增 8 行
- **AND** r5-progress.md 保持 8 行不变
- **AND** r6-progress.md 保持 8 行不变
- **AND** r7-progress.md 保持 8 行不变

### Requirement: 反例档案跨轮次共用

`r5-anti-patterns.md` 是 R5+R6+R7+R8 MUST 共享的反例档案。R8 如有违规案例（如 NOTES.md 工程对照字数不足），追加到该文件，不删除既有案例。

#### Scenario: 追加 R8 违规案例

- **WHEN** R8 验证中发现某卡 NOTES.md 工程对照 <100 字
- **THEN** 在 r5-anti-patterns.md 底部追加新条目
- **AND** 不修改 R5/R6/R7 的既有案例

