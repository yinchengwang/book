# 微信小程序原生化整体重构计划 · 总纲

> 日期：2026-10-22
> 范围：`wechat-miniprogram/` 全部应用原生化 + 旧 Taro Web 源移除 + 全仓引用修正
> 子计划：[执行计划（任务级清单）](./2026-10-22-miniprogram-native-rewrite-execution-plan.md) · [games 已交付] · [programmer-workbench 方案](./2026-10-22-programmer-workbench-native-rewrite-design.md) · [UI 设计语言](./2026-10-22-devbench-ui-design.md)

> **⚠️ 执行结果回填（2026-10-22 完成）**
> 本总纲关于「`engineering/apps/` 整目录删除」的决策**已被执行时的实测推翻并修正**：
> 该目录含活跃 C++ 构建（`apps/CMakeLists.txt` 编译 common / games / tools / vdb_cli / todo-app /
> multimodal_rag，31 个 target），整删会破坏 CMake。经用户当日重新确认，改为**只删除 3 个已迁移的
> Web 源**（games-mini-program / knowledge_hub / reading-radar），保留全部 C++ 工程与
> `apps/web/{mrag,todo-app,taro-template}`。
> 下文 §0、§1、§6、§7、§8 中原「整目录删除」的表述均以此为准。详见执行计划的「执行结果回填」。

---

## 0. 目标与终态

把仓库内所有微信小程序统一为**原生小程序**（无 Taro / React / 构建链），开发者工具打开即跑；移除已迁移的旧 Taro Web 源；全仓文档引用同步修正。

> 修正：原文为「`engineering/apps/` 整目录删除（含其中的 C/C++/Python 工程与全部 Web 项目）」。
> 执行时发现 apps 目录仍是活跃 C++ 构建，整删会破坏 CMake，故仅删除 3 个已迁移 Web 源。

### 终态清单

| 应用 | 技术栈 | 状态 |
|---|---|---|
| `wechat-miniprogram/games/` | 原生（37 文件 / 5414 行 / 51 项测试） | ✅ 已交付 |
| `wechat-miniprogram/trip-log/` | 原生 + 云 SDK（仓库既有，未动） | ✅ 保持 |
| `wechat-miniprogram/programmer-workbench/` | 原生（重写，18 页 / 315 断言） | ✅ 已交付 |
| `wechat-miniprogram/knowledge-hub/` | 原生（重写，17 页 / 181 断言） | ✅ 已交付 |
| `engineering/apps/` | — | ✅ **保留**（活跃 C++ 构建；仅移除 3 个已迁移 Web 源） |

---

## 1. 已确认的全局决策

| 决策项 | 结论 | 确认轮次 |
|---|---|---|
| 总方向 | 全部重写为**原生小程序**，不是迁移 | 2026-10-21 |
| `engineering/apps/` 删除范围 | ~~**整目录全删**~~ → **仅删 3 个已迁移 Web 源**（games-mini-program / knowledge_hub / reading-radar）；保留全部 C++ 工程 | 2026-10-21 初定整删（风险已明示）；**2026-10-22 执行时修正**：实测 apps 仍是活跃 C++ 构建（31 target），整删破坏 CMake |
| 执行节奏 | **串行**：PWB → KH → 统一删除，每步可审阅 | 2026-10-22 |
| KH 重写范围 | **16 页全量**，含 5 个未注册页（quiz / radar / learn_deep / interview_bank / five_year_plan）一并复活并注册进 `app.json`（实际落地 17 页，见 §4 说明） | 2026-10-22 |
| 视觉体系 | **统一设计语言 + 品牌色区分**：DevBench `#0284C7`，Knowledge Hub `#6366F1` | 2026-10-22 |
| 设计基调 | 「冷静的工具感」：4 级字阶、3 档圆角、0.5px 描边分层、零阴影、UI 控件零 emoji | 2026-10-22 |

---

## 2. 阶段总览

```
阶段 0  games 重写 ✅ 已完成（含 3 个原生 bug 修复）
阶段 1  programmer-workbench 实施 ✅ 已完成
阶段 2  knowledge-hub 设计文档 ✅ 已完成
阶段 3  knowledge-hub 实施 ✅ 已完成（17 页 / 14 数据域 / 181 断言）
阶段 4  双应用终验 ✅ 通过（PWB 315 / KH 181 / games 51）
阶段 5  删除已迁移的旧 Taro Web 源 ✅ 已完成（原「删除 engineering/apps」，范围已收窄）
阶段 6  全仓引用修正 + 收尾 ✅ 已完成（commit 4c22b8136）
```

依赖关系：阶段 1 的 P0 产出（设计令牌 + core 基建 + 组件）是阶段 3 的地基——KH 复用同一套 store 工厂、主题体系与组件类。

---

## 3. 阶段 1：programmer-workbench 实施

按既有方案执行（`2026-10-22-programmer-workbench-native-rewrite-design.md`），要点回顾：

- **P0** 骨架：设计令牌落地 `app.wxss`、`core/*`（storage / store 工厂 / theme / seed / codetools / date / format / id）、6 组件（card / tag / empty / progress-bar / header / fab）
- **P1** 工具箱 10 工具 + `tests/core.test.js`
- **P2** 今日页 + 我的
- **P3** 技术 4 模块（8 页，列表卡范式定型）
- **P4** 生活 4 模块（4 页）
- **P5** 项目集 + 闪念 + 全局捕获浮层
- **P6** 测试补齐 + README + 删除旧 `src/`、`dist/`

产出兼作 KH 的基建模板：`core/` 五个模块 + `app.wxss` 令牌体系 + 6 个组件，阶段 3 直接复制改造。

---

## 4. 阶段 2-3：knowledge-hub 设计与实施

> **⚠️ 本节为设计初稿，已被实际实现取代。**
> 权威文档：[knowledge-hub 原生重写设计](./2026-10-22-knowledge-hub-native-rewrite-design.md) ·
> [数据模型清单](./2026-10-22-knowledge-hub-data-model.md)
>
> **实际与本节初稿的差异：**
>
> | 项 | 本节初稿 | 实际实现 |
> |---|---|---|
> | 页面数 | 16 页 | **17 页**（5 tab：首页 / 学习 / 面试 / 知识 / 我的；新增 3 个聚合页，radar+gap 合并、settings 并入 profile） |
> | 数据域 | 沿用 14 个 store | **14 个 `db:` 域**，但剥离 UI/派生态；`projectStore` 改名 `study_projects` 避开与 PWB 作品集同名冲突 |
> | 图表 | `core/charts.js` 一个模块 | 拆为 `graph-layout.js` / `radar-layout.js`（纯函数可单测）+ 两个 Canvas 组件 |
> | 内容数据 | 假设 `@/data/*` 可移植 | **该目录在快照中缺失**，改为自建代表性种子 `core/seed-data.js` |
> | 测试 | — | 6 套 / 181 断言 |

### 4.1 调研结论（已完成）

- **真实注册面 11 页**，`pages/` 下 16 个目录；5 个未注册页约 2700 行是构建外死代码（本计划全部复活）
- **死依赖**：`@dnd-kit`、`framer-motion`、`lucide-react` 完全未使用，直接不移植
- **图表依赖极小**：`recharts` 仅首页 TrendChart 一处；radar 图、知识图谱、热力图日历全部是**手写 SVG / CSS 网格**，可直接移植
- **store 数据面**：14 个 store 共 2307 行；死页面的 store（quizStore / fiveYearPlanStore / interviewBankStore）被首页引用做统计，页面复活后转为完整消费
- **数据层**：Zustand + 本地 storage，本地优先，无服务端依赖（与 PWB 同构，createStore 工厂直接适用）

### 4.2 新目录结构

`wechat-miniprogram/knowledge-hub/`（新目录，不复用旧名 `knowledge_hub`）：

```
knowledge-hub/
├── app.js / app.json / app.wxss     # 16 页注册；tabBar 5 项纯文字
├── core/                            # 从 PWB 复制改造：storage / store / theme / date / format / id
│   ├── md.js                        # 扩展版 md2html（代码块高亮样式）
│   └── charts.js                    # 自绘 canvas 图表：折线 / 雷达
├── components/                      # 复用 PWB 组件 + 新增：
│   ├── heatmap-calendar/            # 热力图日历（CSS 网格，移植）
│   ├── knowledge-graph/             # 知识图谱（canvas 力导向或静态布局）
│   └── markdown-view/               # rich-text + md.js
├── pages/                           # 16 页
└── tests/
```

### 4.3 页面清单（16 页 · 初稿，实际实现为 17 页，见本节开头说明）

| Tab | 页面 | 来源 | 行数 |
|---|---|---|---|
| 首页 | index（含 TrendChart + Heatmap + Markdown） | 已注册 | 443 |
| 复习 | review | 已注册 | 279 |
| 面试 | interview_tracker · mock_interview · interview_bank* | 已注册 + *复活 | 323+614+376 |
| 路径 | learning_path · project_roadmap | 已注册 | 322+297 |
| 设置 | settings | 已注册 | 276 |
| — | knowledge_graph · gap_analysis · excerpt | 已注册 | 350+221+431 |
| — | not-found | 已注册 | 84 |
| — | quiz* · radar* · learn_deep* · five_year_plan* | *复活 | 738+691+504+387 |

tabBar 保持原 5 项（首页 / 复习 / 面试 / 路径 / 设置），改纯文字（弃用旧 PNG 图标，与统一语言一致）。复活页面通过首页与对应模块入口进入。

### 4.4 关键技术决策

| 项 | 决策 |
|---|---|
| 图表 | **自绘 canvas**（折线 + 雷达两个绘制器，约 300 行），零依赖；games 项目已验证 canvas 路线 |
| 知识图谱 | 原为手写 SVG 静态布局 → 移植为 canvas 绘制（节点 + 连线 + 点击命中检测） |
| Markdown | 扩展 PWB 的 `md2html`：补代码块语言标签与高亮配色，输出走 `rich-text` |
| 双主题 | 复用 PWB 的 class 方案；KH 品牌色 `#6366F1` 仅是 `--brand` 默认值不同 |
| store | 14 域全部用 `createStore` 工厂重建，key 沿用语义化命名（新前缀 `kh:`） |
| 复活页验证 | 5 个复活页从未上线，重写完成后**逐页人工走查**，发现的问题按新设计语言修正而非照搬 |

### 4.5 实施批次

| 批次 | 内容 | 页数 |
|---|---|---|
| K0 | 骨架：core 复制改造 + 图表/图谱/markdown 三组件 + 16 页空壳 | — |
| K1 | 首页（最难：图表 + 热力图 + markdown 三合一） | 1 |
| K2 | 复习 + 学习路径 + 项目路线图 | 3 |
| K3 | 面试三页（tracker / mock / bank） | 3 |
| K4 | 知识图谱 + 差距分析 | 2 |
| K5 | 摘录 + 设置 + 404 | 3 |
| K6 | 复活四页（quiz / radar / learn_deep / five_year_plan） | 4 |
| K7 | 人工走查 + `tests/core.test.js` + README | — |

---

## 5. 阶段 4：统一验收门

两个应用全部通过以下检查（games 已达标，作为基准）：

1. `node --check` 全部 `.js` 通过
2. 全部 `.json` 可 `JSON.parse`
3. `tests/*.test.js` 全绿（mock wx API，Node 环境跑真实逻辑）
4. `app.json` 注册页与磁盘文件一一对应
5. **WXML handler 交叉校验**：扫描 `bindtap` / `bindinput` / `catchtap` 绑定与页面 JS 方法比对
6. 每页 4 文件齐全（js / wxml / wxss / json）
7. 复活页人工走查记录（KH 特有）

---

## 6. 阶段 5：删除已迁移的旧 Taro Web 源（原「删除 engineering/apps」）

**前置条件：阶段 4 全部通过。** ✅ 已满足

> **⚠️ 实际执行与原计划不同**。原计划 `git rm -r engineering/apps` 整目录删除。
> 执行时实测 `engineering/apps/CMakeLists.txt` **仍在编译** common / games / tools / vdb_cli /
> todo-app / multimodal_rag（31 个 target），整删会直接破坏 CMake 构建。
> 经用户当日重新确认，收窄为**只删 3 个已迁移的 Web 源**。

实际操作：

```bash
# 先归档（这 3 个目录此前从未被 git 追踪，归档是唯一备份）
tar -czf .archive/engineering-apps-web-migrated-srcs-20261022.tar.gz \
  engineering/apps/web/games-mini-program \
  engineering/apps/web/knowledge_hub \
  engineering/apps/web/reading-radar

# 再删除
git rm -r engineering/apps/web/{games-mini-program,knowledge_hub,reading-radar}
```

- 删除内容：两个 Taro 小程序源码 + reading-radar（knowledge-hub 前身）
- **保留**：`apps/web/{mrag,todo-app,taro-template}`、全部 C++ 工程、`apps/CMakeLists.txt`
- CMake 复验：删除后仍 `Configuring done`，apps targets 31 个不变；体积 17MB → 4.4MB
- 归档：`.archive/engineering-apps-web-migrated-srcs-20261022.tar.gz`（497 文件 / 4MB）

---

## 7. 阶段 6：全仓引用修正

已定位的引用点（删除后逐一处理）。**「实际」列为 2026-10-22 执行结果**：

| 文件 | 位置 | 处理 | 实际 |
|---|---|---|---|
| `engineering/CMakeLists.txt` | L2 注释、L153-154 `add_subdirectory(apps)` | 移除引用与注释中的 `apps/` | **不改** —— apps 仍存在且被使用 |
| 根 `CLAUDE.md` | L380「新独立项目 → 在 `engineering/apps/` 下创建」 | 改为指向新约定 | **不改** —— 约定依然正确 |
| 根 `AGENTS.md` | L79 / L91：api-server 作为 todo-app 统一后端的架构描述 | 标注已移除 | **改法调整** —— api-server 未删，仅修静态目录说明 + `frontend/` 章节改迁移说明 |
| `docs/architecture/reading-radar/README.md` | 指向 `apps/web/reading-radar` | 标注已移除，指向迁移去向 | ✅ 已改 |
| `docs/architecture/knowledge-hub/README.md` | 指向 `apps/web/knowledge_hub` | 标注迁移至 `wechat-miniprogram/knowledge-hub/` | ✅ 已改 |
| `engineering/README.md` | 涉及 apps 的行 | 同步更新 | ✅ 已改 |
| `engineering/apps/games/README.md` | 以 Taro 版为「生产用」 | （计划外补充） | ✅ 已改 |
| `openspec/specs/engineering/{apps-web-knowledge-hub,apps-wechat-mini-program}` | 描述已删模块 | （计划外补充） | ✅ 已加历史规格说明 |
| `wechat-miniprogram/README.md` | 项目清单 | 更新为四应用清单 | ✅ 已改（并注明 trip-log 非零依赖） |
| `docs/superpowers/*` 历史计划文档 | 含 apps 路径 | **不改**——历史文档保持原样 | ✅ 未动正文，仅按用户要求追加执行结果回填 |

---

## 8. 风险清单

| 风险 | 等级 | 缓解 |
|---|---|---|
| 复活 5 页从未上线，重写后行为未知 | 中 | K6 批次单独走查；问题按新设计语言修正而非照搬 |
| 两个应用 core 模块复制后漂移 | 低 | 两份 README 互注来源；模块总量小（约 200 行），漂移成本可控 |
| 删除 apps 误伤 C/C++ 工程 | 高（不可逆感知） | ~~用户两轮确认；git 历史可恢复~~ → **实际执行时该风险已兑现并规避**：发现整删会破坏 CMake，改为仅删 3 个 Web 源并先做 tar 归档（这 3 个目录从未被 git 追踪，归档是唯一备份） |
| 主题行内 CSS 变量真机兼容性 | 中 | 已备退化方案（5 个预设 class）；P0 阶段即真机验证 |
| KH 图表/图谱 canvas 在低端机性能 | 低 | 数据量小（知识点 <100）；静态布局不做物理模拟 |

---

## 9. 明确不做（YAGNI）

- 不保留任何 H5 构建能力（原 KH 与 games 的 H5 端已随 3 个 Taro Web 源删除而消失；注意 `engineering/apps/` 目录本身保留）
- 不迁移旧存储数据（`devbench:` 弃用换 `db:`；KH 直接用 `kh:` 全新开始）
- 不做跨应用代码共享机制（npm 包 / submodule）——微信小程序无法引用项目根外文件，复制是唯一务实解
- 不做自定义 tabBar（两应用首版都用原生纯文字 tabBar，列为可选升级）
- 不引入 UI 组件库、图表库、markdown 库——全部自绘，延续零依赖原则
