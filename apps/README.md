# 微信小程序集合

本目录是仓库内**所有微信小程序的 canonical 位置**。每个小程序一个独立子目录，可直接用微信开发者工具「导入项目」打开。

> ⚠️ 微信小程序**不属于** `../engineering/` track。engineering 存放 C/C++ 库代码与 CMake 工程，两者的构建体系完全不同。
>
> 历史说明：小程序源码原位于 `engine/apps/web/`（Taro 工程），已于 2026-10-22 完成原生化重写并迁入本目录。

---

## 项目清单

| 项目 | 目录 | 技术栈 | 简介 |
|---|---|---|---|
| **trip-log** | [`trip-log/`](./trip-log) | 原生 + 云 SDK | 足迹 —— 游玩记录 |
| **games** | [`games/`](./games) | 原生小程序 | 游戏中心 —— 贪吃蛇 / 数独 / 2048 / 三消 |
| **programmer-workbench** | [`programmer-workbench/`](./programmer-workbench) | 原生小程序 | DevBench —— 程序员个人工作台 |
| **knowledge-hub** | [`knowledge-hub/`](./knowledge-hub) | 原生小程序 | Reading Radar —— 知识管理中心 |

> 三个新应用（games / programmer-workbench / knowledge-hub）均为**零依赖、零构建**：纯 JavaScript + WXML + WXSS，无 npm、无打包步骤，克隆即可跑。
> `trip-log` 例外，它依赖 `@tencent-ai/workbuddy-cloud-sdk`（云服务能力所需）。

---

## 快速开始

用**微信开发者工具导入项目**，选择对应的应用子目录即可（`appid` 默认 `touristappid`，可换成自己的）。

```bash
# 各应用的自测（静态校验 + 单元测试）
cd programmer-workbench && node tests/run-all.js
cd knowledge-hub        && node tests/run-all.js
cd games                && node tests/engine.test.js
```

三个新应用共用同一套基础设施：

- `core/` —— 主题、存储（`db:` 前缀）、数据域工厂、Markdown 渲染等纯逻辑层
- `components/` —— card / tag / empty / progress-bar / header / fab 等通用组件
- `tests/` —— `validate.js` 五项静态校验（JS 语法 / JSON / 页面注册 / 组件引用 / WXML handler 交叉校验）+ 单元测试

---

## trip-log · 足迹

游玩记录微信小程序，基于原生小程序框架 + `@tencent-ai/workbuddy-cloud-sdk`。

```bash
cd trip-log
# 用微信开发者工具直接打开本目录
```

---

## games · 游戏中心

贪吃蛇 / 数独 / 2048 / 俄罗斯方块-match3，纯原生实现，无素材图片（图形全用 Canvas / CSS 绘制）。

```bash
cd games
node tests/engine.test.js    # 游戏引擎单测
```

---

## programmer-workbench · DevBench

程序员个人工作台：工作台 / 技术栈 / 生活 / 项目集 / 工具箱 / 闪念 / 我的，18 个页面。

内置 10 个离线开发工具（JSON、Base64、URL、时间戳、UUID、正则、颜色、Hash、Diff、Cron），数据全部本地化（`wx.setStorageSync` + `db:` 前缀），支持导出/恢复。

详见 [`programmer-workbench/README.md`](./programmer-workbench/README.md)。

---

## knowledge-hub · Reading Radar

知识管理中心：首页 / 学习 / 面试 / 知识 / 我的，17 个页面、14 个数据域。

包含 SM-2 间隔重复复习、题库练习与错题本、面试追踪与模拟面试、知识图谱（Canvas）、技术雷达、月历打卡五年计划等。

详见 [`knowledge-hub/README.md`](./knowledge-hub/README.md)。

---

## 共享代码层

`programmer-workbench` 与 `knowledge-hub` 共享同一套 `core/` 与 `components/`（逐字节相同，修改即分叉）。两者是**独立应用**（tabBar 上限 5 项 + 产品心智不同），只共享代码层，不合并。