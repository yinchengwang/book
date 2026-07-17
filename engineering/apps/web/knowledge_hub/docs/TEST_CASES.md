# Reading Radar 2.0 - 测试用例文档

> 最后更新：2026-06-23 · 版本：2.0

## 一、测试策略

### 1.1 测试金字塔

```
       ┌─────────────────┐
       │   Playwright    │  ← E2E（5 个 spec）
       │   e2e/          │
       ├─────────────────┤
       │  test-pages.cjs │  ← 冒烟（12 页）
       ├─────────────────┤
       │   tsc --noEmit  │  ← 类型检查
       └─────────────────┘
```

### 1.2 测试原则

- **铁律 1**：修改源码后必须通过三层验证（TypeScript / 冒烟 / 构建）
- **铁律 2**：每个新增功能必须有对应的测试用例
- **铁律 3**：测试不能依赖外部状态（除 localStorage）

## 二、自动化测试矩阵

### 2.1 冒烟测试（test-pages.cjs）

覆盖所有 12 个核心页面，验证：
- 页面能成功加载（HTTP 200）
- `.page-container` 文本长度 > 20 字符
- 无 console.error / pageerror

| 页面 | 路径 | TC ID |
|------|------|-------|
| 仪表盘 | `/` | TC-SMOKE-001 |
| 题库练习 | `/quiz` | TC-SMOKE-002 |
| 复习计划 | `/review` | TC-SMOKE-003 |
| 面试追踪 | `/interview_tracker` | TC-SMOKE-004 |
| 模拟面试 | `/mock_interview` | TC-SMOKE-005 |
| 知识图谱 | `/knowledge_graph` | TC-SMOKE-006 |
| 学习路径 | `/learning_path` | TC-SMOKE-007 |
| 图解系列 | `/learn_deep` | TC-SMOKE-008 |
| 摘抄管理 | `/excerpt` | TC-SMOKE-009 |
| 差距分析 | `/gap_analysis` | TC-SMOKE-010 |
| 项目路线 | `/project_roadmap` | TC-SMOKE-011 |
| 设置 | `/settings` | TC-SMOKE-012 |

### 2.2 E2E 测试（e2e/）

详细交互流程：

| Spec 文件 | 测试范围 | TC ID |
|-----------|---------|-------|
| `dashboard.spec.ts` | 仪表盘加载、统计卡、快捷入口 | TC-E2E-001 |
| `interview_tracker.spec.ts` | 面试追踪 CRUD | TC-E2E-002 |
| `review.spec.ts` | 复习计划、卡片翻转 | TC-E2E-003 |

### 2.3 待补充 E2E

| 页面 | 应覆盖的按钮 | 优先级 |
|------|-------------|--------|
| 仪表盘 | 5 卡片 / 7 技术栈卡 / 路线图 | P1 |
| 题库练习 | 开始今日 / 浏览 / 错题重做 / 选项点击 / 提交 | P0 |
| 复习计划 | 3 tab 切换 / 评分按钮 / 上下步 | P1 |
| 面试题库 | Tab 切换 / 分类进入 / 详情 / 复制 | P0 |
| 知识图谱 | 节点点击 / 详情弹窗 / 掌握度调整 | P1 |
| 学习路径 | 路径切换 / step 折叠 / sub-step 勾选 | P1 |
| 图解系列 | 分类筛选 / 搜索 / 详情 / 上下篇 | P2 |
| 摘抄管理 | 书籍筛选 / 标签筛选 / 年份筛选 / 详情 / 复制 | P0 |
| 差距分析 | 空状态显示 / CTA 跳转 | P1 |
| 五年计划 | 月历 / 打卡 / 编辑 modal | P1 |
| 技术雷达 | 8 个主题 Tab / 圆点点击 | P1 |
| 设置 | API key 显示/隐藏/复制 / 主题切换 | P0 |
| 通用（Layout） | 主题切换 / 用户下拉 / 通知跳转 / 搜索 | P0 |

## 三、功能测试用例

### 3.1 UserStore（TC-USER）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-USER-001 | 修改昵称 | 头像首字母自动更新（除非手动设过） |
| TC-USER-002 | 手动设置头像 emoji | 头像保持 emoji，不再自动派生 |
| TC-USER-003 | 修改邮箱 | 持久化 |
| TC-USER-004 | 修改个人简介 | 持久化 |
| TC-USER-005 | 重置用户 | joinDate 保留，其他字段恢复默认 |

### 3.2 NotificationStore（TC-NOTIF）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-NOTIF-001 | dueReviews 有项时 | 显示"今日有 N 张卡片待复习" |
| TC-NOTIF-002 | 错题数 > 0 时 | 显示"错题本有 N 道题" |
| TC-NOTIF-003 | streak > 0 时 | 显示"连续学习 N 天" |
| TC-NOTIF-004 | 点击通知 | 跳转到对应页面 + 标记已读 |

### 3.3 Theme 系统（TC-THEME）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-THEME-001 | 设置页 Switch | 立即切换 dark/light |
| TC-THEME-002 | TopBar 🌙 按钮 | 立即切换 dark/light |
| TC-THEME-003 | light 模式 | 背景浅色、文字深色 |
| TC-THEME-004 | dark 模式 | 背景深色、文字浅色 |
| TC-THEME-005 | 切换后刷新页面 | localStorage 保留主题 |

### 3.4 Sidebar 动态 Badge（TC-SIDEBAR）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-SIDEBAR-001 | 题库 badge | 显示真实题数（2k+） |
| TC-SIDEBAR-002 | 复习 badge | 显示 dueReviews.length |
| TC-SIDEBAR-003 | 面试追踪 badge | 显示进行中公司数 |
| TC-SIDEBAR-004 | 五年计划 badge | 🔥N（N = streak） |
| TC-SIDEBAR-005 | streak = 0 | 底部显示"开始你的学习旅程" |

### 3.5 Settings API Key（TC-SETTINGS）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-SETTINGS-001 | API key 默认 mask | 显示圆点 |
| TC-SETTINGS-002 | 点击"显示" | 明文显示 |
| TC-SETTINGS-003 | 点击"隐藏" | 恢复 mask |
| TC-SETTINGS-004 | 点击"复制" | 写入剪贴板 + 显示"✓ 已复制" |
| TC-SETTINGS-005 | API key 为空时复制 | 显示"✗ 失败" |

### 3.6 摘抄管理（TC-EXCERPT）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-EXCERPT-001 | 首次加载 | 显示 149 条迁移数据 |
| TC-EXCERPT-002 | 书籍筛选 | 只显示该书摘抄 |
| TC-EXCERPT-003 | 标签筛选 | 只显示该标签摘抄 |
| TC-EXCERPT-004 | 年份筛选 | 只显示该年摘抄 |
| TC-EXCERPT-005 | 搜索"指针" | 命中相关摘抄 |
| TC-EXCERPT-006 | 点击卡片 | 弹详情 modal |
| TC-EXCERPT-007 | 复制按钮 | 写入剪贴板 |
| TC-EXCERPT-008 | 状态切换 | 在读→已读→掌握循环 |
| TC-EXCERPT-009 | 添加新摘抄 | 立即出现在列表头部 |
| TC-EXCERPT-010 | 删除摘抄 | 从列表移除 |

### 3.7 GapAnalysis 空状态（TC-GAP）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-GAP-001 | 无答题数据 | 显示空状态 + CTA 按钮 |
| TC-GAP-002 | 点击 CTA | 跳转到 /quiz |
| TC-GAP-003 | 有答题数据 | 显示 5 领域差距 |
| TC-GAP-004 | urgent 领域 | 黄色边框 + 建议 |

### 3.8 Review completedList（TC-REVIEW）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-REVIEW-001 | 待复习 Tab | 显示 dueReviews |
| TC-REVIEW-002 | 全部计划 Tab | 显示所有 records |
| TC-REVIEW-003 | 已完成 Tab | 显示已掌握 records |
| TC-REVIEW-004 | 评分按钮（0-5） | SM-2 算法更新 nextReview |
| TC-REVIEW-005 | 已延期记录 | 显示 ⚠️ 红色标记 |
| TC-REVIEW-006 | 顶部统计 4 卡 | 显示 4 个数字 |

### 3.9 Quiz 分类（TC-QUIZ）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-QUIZ-001 | plan 模式 | 显示 stack 卡片（含真实题数） |
| TC-QUIZ-002 | 点击 stack 卡片 | 进入 list 模式 + 筛选该 stack |
| TC-QUIZ-003 | list 模式 | 三组 filter chip |
| TC-QUIZ-004 | 答题模式 | 选项点击 → 提交 → 反馈 |
| TC-QUIZ-005 | 错题本 | 显示错题记录 |
| TC-QUIZ-006 | 统计 | 显示 4 项统计 |

### 3.10 KnowledgeGraph（TC-KG）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-KG-001 | 加载 | 显示 14 个节点 + SVG 连线 |
| TC-KG-002 | 点击节点 | 弹详情 modal |
| TC-KG-003 | 详情 modal 调整 level | 节点大小实时变化 |
| TC-KG-004 | 点击关联节点 | 跳转到对应节点 |
| TC-KG-005 | 图例 | 5 个领域颜色编码 |

### 3.11 LearningPath sub-step（TC-LP）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-LP-001 | 路径切换 | 切换 3 条路径 |
| TC-LP-002 | step 折叠 | 默认折叠 |
| TC-LP-003 | step 展开 | 显示 sub-step 列表 |
| TC-LP-004 | sub-step 勾选 | 完成状态切换 |
| TC-LP-005 | 进度条 | 基于 sub-step 完成数计算 |
| TC-LP-006 | "完成本步骤"按钮 | 进入下一步 |
| TC-LP-007 | 重置确认 | 弹 modal + 清空 |

### 3.12 Radar 多主题（TC-RADAR）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-RADAR-001 | 默认主题 | 书籍雷达 |
| TC-RADAR-002 | 切换到 C 雷达 | 8 维度多边形 |
| TC-RADAR-003 | 切换到 VDB 雷达 | 紫色多边形 |
| TC-RADAR-004 | 点击圆点 | 弹详情 |
| TC-RADAR-005 | 书籍详情 | 显示作者/标签/描述 |
| TC-RADAR-006 | 维度详情 | 显示掌握度+建议 |
| TC-RADAR-007 | 碰撞检测 | 8 轮迭代，圆点不重叠 |

### 3.13 LearnDeep（TC-LD）

| TC ID | 用例 | 预期 |
|-------|------|------|
| TC-LD-001 | 双栏布局加载 | H5 显示侧边栏 + 内容区 |
| TC-LD-002 | 9 个分类显示 | Linux/网络/系统/数据库/向量数据库/算法/C/Python/C++ |
| TC-LD-003 | 分类折叠展开 | 点击切换折叠状态 |
| TC-LD-004 | 分类筛选 | 点击分类高亮，列表过滤 |
| TC-LD-005 | 搜索 | 实时过滤标题/摘要 |
| TC-LD-006 | 点击文章 | 进入阅读器，显示正文 |
| TC-LD-007 | 面包屑点击 | 跳转回分类 |
| TC-LD-008 | 上一篇/下一篇 | 同分类内按顺序导航 |
| TC-LD-009 | 返回列表 | 点击面包屑返回 |

## 四、错误处理测试

### 4.1 全局错误

| TC ID | 触发条件 | 预期 |
|-------|---------|------|
| TC-ERR-001 | 抛错同步代码 | 控制台打印 + window.__lastError |
| TC-ERR-002 | Promise 拒绝 | 控制台打印 + window.__lastRejection |
| TC-ERR-003 | 组件渲染错误 | ErrorBoundary 显示错误页 + 重试按钮 |

### 4.2 持久化错误

| TC ID | 触发条件 | 预期 |
|-------|---------|------|
| TC-ERR-004 | localStorage 损坏 | 自动用内存 fallback + warn 日志 |
| TC-ERR-005 | 数据迁移失败 | 走默认初始数据 |

## 五、性能基准

| 指标 | 目标 | 实测 |
|------|------|------|
| 首屏渲染 | < 2s | TBD |
| 路由切换 | < 200ms | TBD |
| 雷达图碰撞迭代 | < 50ms | TBD |
| 摘抄筛选（149 条） | < 50ms | TBD |
| localStorage 写入 | < 10ms | TBD |

## 六、运行测试

```bash
# 1. 类型检查
cd c:\code\book\knowledge_hub
bunx tsc --noEmit

# 2. 冒烟测试（启动 dev:h5 后）
cd web
node scripts/test-pages.cjs

# 3. E2E（需先启动 dev:h5）
cd c:\code\book\knowledge_hub
bun run test:e2e

# 4. 完整 E2E 脚本（含依赖安装 + 构建）
bash test-e2e.sh
```

## 七、新增功能测试流程

1. 实现功能
2. 写测试用例（添加到本文档）
3. 跑 `tsc --noEmit`
4. 跑 `test-pages.cjs`
5. 跑 `npm run test:e2e`
6. 跑 `npm run build:h5` + `npm run build:weapp`
7. 全部通过后 commit + push

如有任何测试失败，**禁止提交**，先修复。