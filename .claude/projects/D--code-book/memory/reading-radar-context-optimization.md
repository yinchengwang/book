---
name: reading-radar-context-optimization
description: reading-radar 排查纪律——先 grep 后 Read offset、禁止完整读大文件、browser_snapshot 必须限 depth
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5b0a6a0c-3271-4756-a7c4-5726838c519f
---

# reading-radar 上下文排查守则

## 纪律 1：先 grep 后 Read offset

排查 ≥2 个问题时，对每个文件先 grep 定位关键字（函数名/错误特征），再用 Read(offset, limit≤50) 精确读目标段。

**Why**: 上次排查 3 个问题，完整读了 quiz-system.html(2916行) + index.html(2141行)，直接撑爆上下文。实际上 grep 找 `getQuadrantAngleRange` / `groups = {` / `mode-btn` 三个关键字，每个读 5-10 行就能定位根因。

**How to apply**:
- 排查模式的第一步永远是 grep，不是 Read
- 一次只读一个文件，读完确认再读下一个
- Read 前自问：我 grep 了没有？
- 排查完向用户报告根因时，只陈述发现，不带 Read 的全文
- 哪怕只有 1 个问题，也禁止完整读大文件——server.js(683行) 就是反面教材

**禁止完整读取的大文件**: quiz-questions-*.js (2000-4000行), quiz-system.html (2916行), index.html (2134行), server.js(683行), learn.html(1398行), items-registry.js(370行)

## 纪律 2：browser_snapshot 必须限制 depth

使用 `browser_snapshot` 验证页面状态时，必须加 `depth: 3`（或 ≤5），禁止无限制 dump 页面全部内容。

**Why**: 一次 browser_snapshot 结果可能包含整篇深度文章（~1000 行 YAML），相当于额外读了 ~30KB 无用内容。一次排查调了 4 次 snapshot，直接耗光上下文预算。

**How to apply**:
- 每次 browser_snapshot 加 `depth: 3`：`browser_snapshot({ target: "#sec-deepdive", depth: 3 })`
- 需要确认具体文本内容时，用 `browser_evaluate` 或 Playwright 的 `page.evaluate()` 拿关键文本片段，不 dump 整个渲染树
- 例如确认文章渲染成功：`browser_evaluate({ function: "() => document.querySelector('.markdown-body h2')?.textContent" })`
- 只在首次了解页面结构时用 1 次完整 snapshot（depth 不加），后续验证全用 depth ≤ 3

## 纪律 3：长篇内容生成——避免范文完整读 + 分批 compact

生成深度文章（如 learn-deep 系列的 .md 文件）时，遵守以下规则：

### 3.1 范文只读模板结构，不读全文

**Why**: 本轮读取 513 行 `db-vector-basic.md` 范文来确定风格，但实际上该文件的风格已在之前对话中熟悉。只需要读前 60 行（到第一个完整代码块或表格）就能摸清模板结构：`# 标题 → > 引言 → --- → ## 一、... → ## 二、... → 代码块 → 表格 → --- → ## 参考 → > 总结`。

**How to apply**:
- 已有同系列文章时，先自问：我是否已经知道这个系列的格式？
- 如果需要确认风格，Read offset=0 limit=60 （或 grep 定位关键结构标记，如 `## 一、` / `## 参考`）
- 禁止因"想写得更像"而完整读任何已生成的长文

### 3.2 批量生成长文时，每 2 篇后主动 compact

**Why**: 5 篇连续生成（每篇 ~800 行），写到第 5 篇时前 4 篇的完整输出仍在上文，累积占用 ~40% 上下文。如果每 2 篇 compact 一次，后续文章的上下文会干净很多，总消耗可降低 ~25-30%。

**How to apply**:
- 预计生成 ≥3 篇长篇（每篇 >500 行）时，每写完 2 篇执行一次 `/compact`
- 或主动说"我先 compact 一下再继续"（使用 ScheduleWakeup 或自然断点）
- 不要一口气写完整批再回头看效果

### 3.3 TodoWrite 批量更新而非逐次更新

**Why**: 6 次 TodoWrite × 6 个项 = 36 条状态项在来回传递。虽然单次不大，但累积多次的往返开销不可忽视。

**How to apply**:
- 批量生成场景：一次性初始化 todos，写完所有文章后再一次性标记完成
- 避免"写完一篇 → 更新 todo → 写下一篇"的逐条模式
- 改用"先全部写完 → 再统一更新 tasks.md + todos"

**关联记忆**: [[project_apps_and_tests]] — reading-radar 文件结构全貌
