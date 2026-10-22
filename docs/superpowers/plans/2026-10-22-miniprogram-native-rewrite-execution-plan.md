# 微信小程序原生化重构 · 执行计划

> 日期：2026-10-22
> 上游文档：[总纲](./2026-10-22-miniprogram-native-rewrite-master-plan.md) · [PWB 方案](./2026-10-22-programmer-workbench-native-rewrite-design.md) · [UI 设计语言](./2026-10-22-devbench-ui-design.md)
>
> 用法：按任务编号顺序执行。每个任务列出**产出文件**与**验收命令**；验收不过不进入下一任务。每个阶段结束做一次 git commit。

---

## ⚠️ 执行结果回填（2026-10-22 完成）

本计划已全部执行完毕。**执行中发现两处计划假设与实际不符，已按实际调整并记录如下**，
正文中对应条目已就地标注，不再保留误导性表述。

| 项 | 计划原定 | 实际执行 | 原因 |
|---|---|---|---|
| **阶段 5 删除范围** | `git rm -r engineering/apps`（整目录） | **仅删 3 个已迁移 web 源** | 实测 `engineering/apps/CMakeLists.txt` 仍在编译 common / games / tools / vdb_cli / todo-app / multimodal_rag（31 个 target），整删会破坏 CMake 构建。经用户当日重新确认后收窄范围 |
| **18.1 CMakeLists** | 删 `add_subdirectory(apps)` | **不改** | apps 目录仍存在且被使用，守卫必须保留 |
| **18.2 CLAUDE.md** | 改「新独立项目 → engineering/apps/」 | **不改** | 该约定依然正确（apps 仍是 C++ 独立项目的落点） |
| **18.3 AGENTS.md** | 标注 api-server 已移除 | **保留 api-server，仅修静态目录说明** | api-server 是仍在使用的 C 后端，未删除 |

> 关于「用户曾两次确认整目录删除」：该决策形成于 2026-10-21，前提是认为 `apps/` 只含已迁移的
> Web 项目。2026-10-22 执行时通过 CMake 实测发现前提不成立，用户据此重新选择了收窄范围。
> 因此本计划正文的阶段 5 / 阶段 6 与总纲的相应描述均以「执行结果回填」为准。

**最终验收（全部通过）**

| 应用 | 结果 |
|---|---|
| programmer-workbench | 7 组 / 315 断言 |
| knowledge-hub | 6 组 / 181 断言 |
| games | 51 项 |
| CMake | `Configuring done`，apps targets 31 个不变 |

提交：`4c22b8136`（340 文件 / +35540 / −1034）

---

## 0. 全局约定

### 0.1 核心校验脚本（P0.1 一次性建成，全项目复用）

`tests/validate.js` 放在每个小程序项目内，负责五项静态检查：

| # | 检查 | 实现 |
|---|---|---|
| 1 | JS 语法 | 对每个 `.js` 跑 `new Function` 前置 `node --check`（脚本内用 `child_process`） |
| 2 | JSON 合法 | 逐个 `JSON.parse` |
| 3 | 页面注册一致 | `app.json.pages` 每项在磁盘有对应 4 文件（js/wxml/wxss/json），反向无孤儿 |
| 4 | 组件引用一致 | 页面 `.json` 的 `usingComponents` 路径存在 |
| 5 | **WXML handler 交叉校验** | 正则提取 `bind*` / `catch*` 的方法名，与页面 `.js` 中 `Page({ ... })` 内的方法名比对，缺失即报错 |

```bash
# 用法（每个应用目录内）
node tests/validate.js
```

### 0.2 目录与提交约定

- 新原生文件一律写在应用根目录；旧 Taro 文件（`src/`、`dist/`、`config/` 等）**保留到最后阶段统一删除**，期间作为参照源
- `project.config.json` 的 `packOptions.ignore` 全程忽略 `src` / `dist` / `tests` / `node_modules`
- 每阶段一个 commit，message 格式 `native-rewrite(<app>): <phase> <一句话>`

### 0.3 设计令牌速查（两个应用共用，仅 `--brand` 不同）

```
浅: 页面底 #F7F8FA / 卡片 #FFFFFF / 内嵌 #F1F3F5 / 描边 #E4E7EC / 主文 #0F172A / 次文 #64748B
深: 页面底 #0B0F17 / 卡片 #151A23 / 内嵌 #1D2430 / 描边 #232B38 / 主文 #E6EAF0 / 次文 #94A3B8
品牌: PWB #0284C7(浅)/#38BDF8(深)   KH #6366F1(浅)/#818CF8(深)
语义: 危险 #E5484D / 警告 #F5A524 / 成功 #30A46C / 辅助 #7F77DD
字阶: display 22 / title 17 / body 14 / caption 12 （字重仅 400/500）
间距: 4/8/12/16/24 ；圆角: 卡 12 / 控件 8 / 胶囊 999 ；分层: 0.5px 描边，零阴影
```

---

## 阶段 1 · programmer-workbench

### P0 骨架与基建（约 30 个文件）

- [ ] **1.1 校验脚本**
  产出：`programmer-workbench/tests/validate.js`
  验收：`node tests/validate.js` 能对当前空壳跑通并正确报错（故意删一个文件试一次）
- [ ] **1.2 清理冲突配置**
  产出：改写 `project.config.json`（`miniprogramRoot: "./"`，packOptions 忽略旧目录）、`project.private.config.json`
  验收：开发者工具导入根目录不再报 `src/` 相关错误
- [ ] **1.3 core 纯逻辑层**（移植自 `src/utils/` + `src/stores/` 的非 UI 部分）
  产出：`core/id.js`、`core/date.js`、`core/format.js`、`core/storage.js`（`db:` 前缀 + 导入导出）、`core/store.js`（`createListStore` / `createObjectStore`）、`core/theme.js`（class 解析 + `wx.onThemeChange`）、`core/seed.js`（11 类种子数据，移植 `_bootstrap.ts`）、`core/codetools.js`（10 工具纯函数，320 行原样去类型）
  验收：`node --check core/*.js` 全过
- [ ] **1.4 core 单元测试**
  产出：`tests/core.test.js`（mock `wx` 全局）
  覆盖：store 工厂 CRUD、storage 导入导出 merge/overwrite、codetools 确定性输出（Base64/JSON/时间戳/UUID 格式/hex-rgb/crc32/diff/cron）、date/format
  验收：`node tests/core.test.js` 全绿
- [ ] **1.5 app 骨架**
  产出：`app.json`（18 页注册 + 5 个纯文字 tab：今日/技术/生活/工具/我的）、`app.js`（onLaunch：主题解析 + seed）、`app.wxss`（令牌变量 + `.theme-light` / `.theme-dark` + 6 个通用类：`.card` `.tag` `.btn-primary` `.btn-ghost` `.list-item` `.empty`）
  验收：`validate.js` 通过；开发者工具预览 tab 可切换
- [ ] **1.6 组件库**（每个 `Component()` 四件套）
  产出：`components/{card,tag,empty,progress-bar,header,fab}/`
  验收：`validate.js` 组件引用检查通过
- [ ] **1.7 页面主题包装器**
  产出：`core/with-theme.js`（注入 `onShow` 主题同步 + 根 view class 绑定模式说明）
  验收：任一页面接入后，切换主题设置时变量实时生效
- [ ] **P0 commit**

### P1 工具箱（1 页 + 10 面板）

- [ ] **2.1 工具箱框架**：`pages/tools/`（搜索过滤 + 4 列网格 + 全屏面板层 `wx:if`）
- [ ] **2.2 十个工具面板**：json / base64 / url / timestamp / uuid / regex / color / hash / diff / cron（输入在上、结果在下、一键复制 `wx.setClipboardData`）
- [ ] **2.3 验收**：`validate.js`；手工过每个工具的 编码→解码 往返一致性
- [ ] **P1 commit**

### P2 今日 + 我的（2 页 + 捕获浮层雏形）

- [ ] **3.1 `pages/dashboard/`**：待办卡（3 条可勾选 + 优先级圆点）→ 4 指标带 → 心情打卡 → 4 快捷入口
- [ ] **3.2 `pages/profile/`**：资料编辑 / 主题三态 / 主色 5 选 / 导出（`wx.setClipboardData` + 保存对话）/ 恢复（merge/overwrite 二选一）/ 清空数据
- [ ] **3.3 验收**：`validate.js`；导出→清空→恢复 往返数据一致
- [ ] **P2 commit**

### P3 技术模块（8 页，列表卡范式定型）

- [ ] **4.1 `pages/tech/index/`**：4 入口 + 计数
- [ ] **4.2 笔记**：`notes/index`（列表卡 + 左色条分类色）+ `notes/detail`（编辑 + `rich-text` Markdown 预览切换）
- [ ] **4.3 片段**：`snippets/index` + `snippets/detail`（等宽字体展示 + 复制）
- [ ] **4.4 面试题**：`interview/index` + `interview/detail`（难度色 + 收藏 + 复习计数）
- [ ] **4.5 资源**：`resources/index`（类型徽标 + 星级 + 归档）
- [ ] **4.6 验收**：`validate.js`；四模块各自 增→改→删 往返
- [ ] **P3 commit**

### P4 生活模块（4 页）

- [ ] **5.1 `pages/life/index/`** 聚合
- [ ] **5.2 reading / fitness / mood / schedule** 四页（书进度条、周运动合计、五级心情、日程列表）
- [ ] **5.3 验收**：`validate.js`；mood 每日一条幂等（`upsertToday`）
- [ ] **P4 commit**

### P5 项目集 + 闪念 + 捕获浮层（2 页 + 1 组件）

- [ ] **6.1 `pages/projects/`**（技术栈标签 + 状态徽标）
- [ ] **6.2 `pages/insights/`**（时间线 + 置顶）
- [ ] **6.3 全局捕获浮层**：`components/capture-sheet/`，tab 页接入 FAB 长按/点按 → 待办 / 闪念 / 笔记三选
- [ ] **6.4 验收**：`validate.js`；任意 tab 唤起浮层并成功写入对应 store
- [ ] **P5 commit**

### P6 收尾

- [ ] **7.1 删除旧工程**：`git rm -r src dist config package.json pnpm-lock.yaml babel.config.js tsconfig.json .npmrc .swc`
- [ ] **7.2 README 重写**（原生说明 + 目录结构 + `db:` 键名表）
- [ ] **7.3 全量验收**：`node tests/validate.js` + `node tests/core.test.js` + 开发者工具人工冒烟（启动/主题/增删改查/导出恢复）
- [ ] **P6 commit**

---

## 阶段 2-3 · knowledge-hub（`wechat-miniprogram/knowledge-hub/` 新目录）

### K0 骨架（复制改造 PWB 基建）

- [ ] **8.1** `core/` 从 PWB 复制：`storage.js`（前缀改 `kh:`）、`store.js`、`theme.js`、`date.js`、`format.js`、`id.js`、`with-theme.js`；新增 `core/md.js`（扩展 md2html：围栏代码块 + 语言标签 + 高亮配色）、`core/charts.js`（自绘 canvas：折线 + 雷达两个绘制器）
- [ ] **8.2** `tests/validate.js` 复制 + `tests/core.test.js`（md 渲染、store、storage）
- [ ] **8.3** `app.json` 16 页注册 + 5 纯文字 tab（首页/复习/面试/路径/设置）、`app.wxss` 令牌（品牌 `#6366F1`）、6 组件复制
- [ ] **8.4** 三个新组件：`components/heatmap-calendar/`（CSS 网格移植）、`components/knowledge-graph/`（canvas）、`components/markdown-view/`
- 验收：`node tests/validate.js` + `node tests/core.test.js`

### K1 首页（最难，三合一）

- [ ] **9.1** `pages/index/`：TrendChart（canvas 折线）+ HeatmapCalendar + MarkdownRenderer 合体 + 各模块统计卡
- [ ] **9.2** 验收：图表数据驱动渲染正确；空数据不崩

### K2 复习线（3 页）

- [ ] **10.1** `pages/review/`（复习队列 + 掌握度）、`pages/learning_path/`（路径节点，494 行 store 逻辑最重）、`pages/project_roadmap/`
- [ ] 验收：`validate.js`

### K3 面试线（3 页）

- [ ] **11.1** `pages/interview_tracker/`（tab）+ `pages/mock_interview/`（614 行）+ `pages/interview_bank/`（复活页）
- [ ] 验收：`validate.js`

### K4 图谱与分析（2 页）

- [ ] **12.1** `pages/knowledge_graph/`（canvas 图谱：节点布局 + 连线 + 点击命中）、`pages/gap_analysis/`
- [ ] 验收：`validate.js`

### K5 摘录 + 设置 + 404（3 页）

- [ ] **13.1** `pages/excerpt/`（431 行）、`pages/settings/`、`pages/not-found/`
- [ ] 验收：`validate.js`

### K6 复活四页（从未上线，单独走查）

- [ ] **14.1** `pages/quiz/`（738 行，测验逻辑）、`pages/radar/`（691 行，canvas 雷达）、`pages/learn_deep/`（504 行，ArticleReader 深读）、`pages/five_year_plan/`（387 行）
- [ ] **14.2** 走查规则：发现问题按新设计语言修正，**不照搬**旧行为；无对应交互预期的简化或砍掉并记录
- [ ] 验收：`validate.js` + 每页走查记录（写入 KH README）

### K7 收尾

- [ ] **15.1** `tests/core.test.js` 补齐图表绘制器（数据→坐标断言）
- [ ] **15.2** README（16 页清单 + `kh:` 键名表 + 走查记录）
- [ ] **15.3** 全量验收 + commit

---

## 阶段 4 · 双应用统一终验

- [ ] **16.1** 两个应用各跑：`node tests/validate.js`、`node tests/*.test.js`
- [ ] **16.2** 开发者工具双应用冒烟：启动 → tab 切换 → 每模块 增删改查 → 主题切换 → 深浅色 → 导出恢复
- [ ] **16.3** 复活页走查记录复核（KH 5 页）
- [ ] **16.4** 全部通过后才进入删除

---

## 阶段 5 · 删除已迁移的旧 Taro 源（原为「删除 engineering/apps」）

> **⚠️ 范围已修正**：原计划 `git rm -r engineering/apps` 整目录删除。实测该目录含活跃 C++ 构建
> （`apps/CMakeLists.txt` 编译 common / games / tools / vdb_cli / todo-app / multimodal_rag，共 31 个
> target），整删会破坏 CMake。经用户重新确认，收窄为**只删已迁移的 3 个 web 源**。

- [x] **17.1** 删除已迁移的 3 个 Web 源（**非**整目录）：
  - `engineering/apps/web/games-mini-program` → 已迁移至 `wechat-miniprogram/games`
  - `engineering/apps/web/knowledge_hub` → 已迁移至 `wechat-miniprogram/knowledge-hub`
  - `engineering/apps/web/reading-radar` → knowledge-hub 前身，已被取代
  - 保留：`apps/web/{mrag,todo-app,taro-template}` 与全部 C++ 工程
- [x] **17.1b** 删除前归档：`.archive/engineering-apps-web-migrated-srcs-20261022.tar.gz`（497 文件 / 4MB）
  - **重要**：这 3 个目录此前**从未被 git 追踪**（不在任何 commit 中），归档是唯一备份
- [x] **17.2** 确认 CMake 不受影响：删除后 `cmake -S engineering -B <build>` 仍 `Configuring done`，
  apps targets 31 个不变。体积 17MB → 4.4MB
- [x] **17.3** 随最终 commit 一并提交

---

## 阶段 6 · 全仓引用修正

| # | 文件 | 动作 | 实际 |
|---|---|---|---|
| 18.1 | `engineering/CMakeLists.txt` | 删 L2 注释中的 `apps/`、删 L153-154 `if(EXISTS)…add_subdirectory(apps)…endif()` | **不改** —— apps 仍存在且被使用，守卫必须保留 |
| 18.2 | 根 `CLAUDE.md` L380 | 「新独立项目 → `engineering/apps/`」改为 engineering 直属目录约定 | **不改** —— 该约定依然正确 |
| 18.3 | 根 `AGENTS.md` L79 / L91 | api-server（todo-app C 后端）章节标注已移除；构建路径示例删除 | **改法调整** —— api-server 未删除，仅修静态目录说明（默认值指向已删的 `frontend/apps/knowledge_hub/web/dist`，改为需 `-s` 显式指定）；另将 `frontend/` monorepo 章节改为迁移说明 |
| 18.4 | `engineering/README.md` | 同步 apps 相关行 | ✅ 已改（apps 清单补全） |
| 18.5 | `docs/architecture/reading-radar/README.md` | 顶部加迁移说明（已删除，原型见 git 历史） | ✅ 已改 |
| 18.6 | `docs/architecture/knowledge-hub/README.md` | 顶部加迁移说明（现位于 `wechat-miniprogram/knowledge-hub/`） | ✅ 已改 |
| 18.7 | `wechat-miniprogram/README.md` | 更新为四应用清单（trip-log / games / programmer-workbench / knowledge-hub） | ✅ 已改（并注明 trip-log 依赖云 SDK，非零依赖） |
| 18.8 | `docs/superpowers/*` 历史计划 | **不改**（历史文档保持原样） | ✅ 未动正文；仅按用户要求追加「执行结果回填」区块 |
| 18.9a | `engineering/apps/games/README.md` | （计划外补充） | ✅ 已改 —— 原描述以 Taro 版为「生产用」，改为原生小程序 |
| 18.9b | `openspec/specs/engineering/{apps-web-knowledge-hub,apps-wechat-mini-program}` | （计划外补充） | ✅ 已加历史规格说明 |

- [x] **18.9** 全仓 grep 复查：源码/文档中已无指向已删路径的有效引用（仅剩迁移说明与历史记录）
- [x] **18.10** 最终 commit：`4c22b8136`

---

## 完成定义（Definition of Done）

> 下列为**修订后**的验收标准。原第 1、3 条按字面无法成立，已改写为与实际一致且仍可检验的表述；
> 原措辞保留在括注中，便于追溯。

1. `wechat-miniprogram/` 下四个应用均可用开发者工具直接导入运行。
   **其中 3 个（games / programmer-workbench / knowledge-hub）零 npm 依赖、零构建**；
   `trip-log` 例外，依赖 `@tencent-ai/workbuddy-cloud-sdk`（云能力所需，非本次重写范围）。
   *（原文：「四个应用全部原生、零 npm 依赖」—— 对 trip-log 不成立）*
2. 每个应用 `validate.js` + 单测全绿。**实测：PWB 7 组 / KH 6 组 / games 51 项，全通过。**
3. 已迁移的 3 个旧 Taro Web 源已删除；全仓（历史文档除外）无指向它们的失效引用。
   **`engineering/apps/` 目录整体保留** —— 它仍是活跃的 C++ 构建（31 个 target），
   计划原定的「整目录删除」因会破坏 CMake 而未执行（详见「执行结果回填」）。
   *（原文：「`engineering/apps/` 不存在」—— 已修正）*
4. 三个新应用 README 完整（结构 / 存储 / 测试 / 移植对照）。**✅ 已完成。**
