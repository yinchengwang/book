# e2e-coverage

## Purpose

为 Reading Radar 2.0 H5 端的 11 个核心页面提供 Playwright 自动化 E2E 测试覆盖，确保每个可点击的按钮都被测试断言。

## Requirements

### Requirement: 11 页面 E2E 覆盖

每个核心页面必须有对应的 `e2e/<page>.spec.ts` 文件：

| 页面 | spec | 优先级 | 覆盖按钮 |
|------|------|--------|----------|
| Dashboard | `dashboard.spec.ts`（已存在，需扩） | P0 | 打卡 4 项 / 查看详细计划 / 4 统计卡 / 7 stack / 5 年路线 |
| Quiz | `quiz.spec.ts`（新建） | P0 | 4 模式切换 / stack/category/difficulty 筛选 / 选项 / 提交 / 退出 |
| Review | `review.spec.ts`（已存在，需扩） | P0 | 3 tab / 6 评分按钮 / 上一步/下一步 / 重置 |
| Interview Tracker | `interview-tracker-full.spec.ts` | P0 | 新建/删除/编辑/14 阶段切换/详情 |
| Mock Interview | `mock-interview-full.spec.ts` | P1 | 配置/答题/反馈 |
| Knowledge Graph | 已有 | P1 | 节点点击 / 详情 / 掌握度调整 |
| Learning Path | `learning-path.spec.ts` | P1 | 路径切换 / 折叠 / sub-step 勾选 / 重置 |
| Learn Deep | `learn-deep.spec.ts` | P1 | 分类 / 搜索 / 详情 / 上下篇 |
| Excerpt | `excerpt.spec.ts` | P0 | CRUD / 三维筛选 / 状态切换 |
| Gap Analysis | `gap-analysis.spec.ts` | P1 | 空状态 / 有数据 / CTA |
| Radar | `radar.spec.ts` | P1 | 8 主题 / 圆点 / 详情 |
| Interview Bank | `interview-bank-full.spec.ts` | P0 | 分类 / 详情 / 复制 |
| Settings | `settings.spec.ts` | P0 | API key / 主题 / 导出 |
| TopBar/Layout | `topbar-layout.spec.ts` | P0 | Sidebar / TopBar / 主题 / 通知 / 用户 |

#### Scenario: 每个按钮都有 E2E 断言
- **WHEN** 用户访问任一页面
- **THEN** 该页面所有可点击元素（`<View onClick>` / `<Button onClick>` / `<a href>`）至少有 1 个 E2E 断言覆盖
- **AND** 断言包含：点击 → 验证副作用（导航 / 状态变化 / API 调用 / 弹窗）

#### Scenario: 失败重试机制
- **WHEN** 任意 spec 失败
- **THEN** Playwright 报告失败用例 + 截图 + 错误堆栈
- **AND** CI 退出码非 0

### Requirement: 测试基础设施

复用 `reading-radar-2/web/scripts/test-pages.cjs` 的 headless_shell 启动方式：

```ts
const browser = await chromium.launch({
  headless: true,
  executablePath: 'C:\\Users\\yinch\\AppData\\Local\\ms-playwright\\chromium_headless_shell-1223\\chrome-headless-shell-win64\\chrome-headless-shell.exe',
  args: ['--no-sandbox', '--disable-dev-shm-usage', '--disable-gpu']
})
```

#### Scenario: Playwright 浏览器启动成功
- **WHEN** 执行 `bun run test:e2e`
- **THEN** 浏览器在 30s 内启动
- **AND** 14 个 spec 全部运行完成

### Requirement: 测试覆盖矩阵

11 个新增 spec 的最小覆盖要求：

- 至少 1 个 happy path
- 至少 1 个 error path（如空状态 / 越界）
- 至少 1 个数据持久化断言（如 localStorage 写入验证）

#### Scenario: 全量 E2E 通过
- **WHEN** `bun run test:e2e` 运行
- **THEN** 14 个 spec 全部通过（dashboard + interview-tracker + review + 11 新增）
- **AND** 总耗时 < 5 分钟