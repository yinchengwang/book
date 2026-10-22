# DevBench · 程序员个人工作台

> 一站式个人工作台：技术栈学习、生活记录、项目作品、工具箱、闪念便签。

为开发者量身定制的「个人 OS」。7 大板块把日常的笔记、代码片段、面试题、资源收藏、读书、健身、心情、日程、项目展示、闪念收纳到一个**本地优先**的小程序里。

**技术栈：原生微信小程序，零 npm 依赖，零构建链。** 开发者工具打开即跑。

---

## 快速开始

```
1. 微信开发者工具 → 导入项目
2. 目录选择：wechat-miniprogram/programmer-workbench
3. AppID 选择「测试号」或填自己的
4. 直接编译运行，无需 npm install / build
```

项目用 `touristappid`（游客模式），无任何网络请求，不需要配置服务器域名。

---

## 目录结构

```
programmer-workbench/
├── app.js / app.json / app.wxss     # 入口、18 页注册、设计令牌
├── project.config.json              # 工具配置（miniprogramRoot: ./）
├── core/                            # 纯逻辑层（无 wx 依赖，可在 Node 下测试）
│   ├── storage.js                   # 本地存储封装（db: 前缀、导入导出）
│   ├── store.js                     # createListStore / createObjectStore 工厂
│   ├── stores.js                    # 12 个数据域实例
│   ├── seed.js                      # 首次启动的示例数据
│   ├── codetools.js                 # 10 个工具纯函数（Base64 自实现）
│   ├── md.js                        # Markdown → rich-text 内联样式 HTML
│   ├── theme.js                     # 主题解析 + 导航栏/tabBar 同步
│   ├── with-theme.js                # 页面主题包装器
│   ├── constants.js                 # 分类 / 优先级 / 状态映射
│   ├── date.js / format.js / id.js
├── components/                      # 7 个组件
│   ├── card / tag / empty / progress-bar / header / fab
│   └── capture-sheet                # 全局捕获浮层
├── pages/                           # 18 页
└── tests/                           # 315 项测试 + 静态校验
```

---

## 页面（18）

| Tab | 页面 |
|---|---|
| 今日 | `dashboard` |
| 技术 | `tech`（聚合）· `tech/notes` 列表+详情 · `tech/snippets` 列表+详情 · `tech/interview` 列表+详情 · `tech/resources` |
| 生活 | `life`（聚合）· `life/reading` · `life/fitness` · `life/mood` · `life/schedule` |
| 工具 | `tools`（10 个工具） |
| 我的 | `profile` |
| — | `projects` · `insights` |

---

## 存储键名

所有数据在 `wx.storage`，统一 `db:` 前缀。**不兼容旧 Taro 版的 `devbench:` 前缀**。

| 键 | 内容 |
|---|---|
| `db:todos` | 待办 |
| `db:tech_notes` | 技术笔记 |
| `db:snippets` | 代码片段 |
| `db:interview` | 面试题库 |
| `db:resources` | 资源收藏 |
| `db:reading` | 读书 |
| `db:fitness` | 健身记录 |
| `db:mood` | 心情（每日一条，同日覆盖） |
| `db:schedule` | 日程 |
| `db:projects` | 项目集 |
| `db:insights` | 闪念 |
| `db:settings` | 主题、主色、资料、开关 |

首次启动时各域自动写入示例数据；在「我的 → 清空全部数据」后会重新写入。

---

## 设计语言

「冷静的工具感」——克制的色彩、精确的间距、清晰的层级。不做消费级的活泼，也不做企业级的刻板。

| 令牌 | 浅色 | 深色 |
|---|---|---|
| 页面底 | `#F7F8FA` | `#0B0F17` |
| 卡片 | `#FFFFFF` | `#151A23` |
| 描边 | `#E4E7EC` | `#232B38` |
| 主文 / 次文 | `#0F172A` / `#64748B` | `#E6EAF0` / `#94A3B8` |

- **字阶只有四级**：display 22 / title 17 / body 14 / caption 12（字重仅 400 与 500）
- **圆角三档**：卡片 12 / 控件 8 / 胶囊 999
- **分层靠 0.5px 描边，零阴影**
- **图标零资源**：全部纯 CSS 绘制或等宽符号，不引入任何图片
- **列表卡统一范式**：左侧 6rpx 色条承载优先级/分类语义，整卡可点、卡内无按钮

主题实现：WXSS 不支持 `[data-theme]` 选择器，改用 `.theme-light` / `.theme-dark` class 承载 CSS 变量；主色通过根 view 行内 `--brand` 覆盖；导航栏与 tabBar 用 `wx.setNavigationBarColor` / `wx.setTabBarStyle` 运行时同步。

---

## 测试

无测试框架依赖，纯 Node 脚本。

```bash
node tests/run-all.js        # 一次跑全部 7 组
```

| 命令 | 内容 | 数量 |
|---|---|---|
| `node tests/validate.js` | 静态校验：JS 语法 / JSON / 页面注册双向一致 / 组件引用 / WXML handler 交叉校验 | — |
| `node tests/core.test.js` | core 层：store 工厂、存储导入导出、10 工具、日期格式化、主题、常量 | 106 |
| `node tests/tools.test.js` | 工具箱 10 个工具的交互与往返一致性 | 48 |
| `node tests/pages.test.js` | 今日页与我的页 | 43 |
| `node tests/tech.test.js` | 技术栈 8 页 + Markdown 渲染 | 48 |
| `node tests/life.test.js` | 生活模块 5 页 | 40 |
| `node tests/p5.test.js` | 项目集、闪念、捕获浮层 | 30 |

合计 **315 项**。页面测试用 `tests/page-harness.js` 加载真实页面模块，模拟 `onLoad` / `onInput` / `onTap`，断言 `data` 与 store 变化——不是快照测试，是真逻辑验证。

**WXML handler 交叉校验**值得单独说：它扫描每个 `.wxml` 里的 `bind*` / `catch*` 绑定，与页面 JS 中的方法名比对。这类错误开发者工具只在运行时提示，静态抓出来能省大量调试。

---

## 已知限制

- **tabBar 为原生纯文字**。设计稿中的选中指示条需要 `custom: true` + 自建组件，列为可选升级项。
- **不支持跨设备同步**。数据仅在本机，迁移靠「我的 → 导出备份」复制 JSON，再在新设备「从备份恢复」。
- **外部链接不可直接打开**。小程序限制，资源与项目的链接只能复制到剪贴板。
- **恢复备份走剪贴板**。超大数据量（数百 KB 以上）可能受剪贴板长度限制，建议分模块导出。

---

## 迁移说明

本项目于 2026-10-22 由 **Taro 3.6 + React 18 + Zustand** 重写为原生小程序，功能完整对等。

- 代码量：旧版 `src/` 约 470 处内联样式、6 级字阶混用；新版统一为设计令牌 + 全局组件类
- 存储：`devbench:` → `db:`，**不读取旧数据**（旧数据需先导出再手工转换）
- 旧 Taro 源码已归档至 `wechat-miniprogram/.archive/programmer-workbench-taro-src-20261022.tar.gz`
- 设计决策见 `docs/superpowers/plans/2026-10-22-devbench-ui-design.md`
- 重写方案见 `docs/superpowers/plans/2026-10-22-programmer-workbench-native-rewrite-design.md`
