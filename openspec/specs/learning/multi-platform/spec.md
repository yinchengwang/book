# multi-platform

## Purpose

Reading Radar 2.0 同时支持 H5 网页端和微信小程序端，两端共享同一套业务代码、状态管理和数据模型。

## Requirements

### Requirement: 双端并行编译

H5 端和小程序端必须能**独立编译、独立运行**：

```bash
# H5 端（Vite）
cd web && npm run dev

# 小程序端（Taro）
npm run build:weapp
```

两套编译链**互不依赖**，可分别升级。

#### Scenario: H5 独立启动

- **WHEN** 开发者执行 `cd web && npm run dev`
- **THEN** 仅 Vite 启动，不触发 Taro 编译链

#### Scenario: 小程序独立构建

- **WHEN** 开发者执行 `npm run build:weapp`
- **THEN** 仅 Taro 启动，不依赖 web/ 目录

### Requirement: 共享源码

`src/` 下的业务代码必须同时被两端消费：

- H5 端通过 Vite alias 引用
- 小程序端通过 Taro 编译引用
- 同一份 `.tsx` 文件，两端都能识别

#### Scenario: 业务页面跨端复用

- **WHEN** 在 `src/pages/quiz/Quiz.tsx` 修改代码
- **THEN** H5 端 Vite HMR 自动更新，且 `npm run build:weapp` 能正确编译小程序版本

#### Scenario: 跨端组件定义

- **WHEN** 在 `src/components/Button/Button.tsx` 创建新组件
- **THEN** 该组件 SHALL 同时被 H5 端和小程序端引用，无需 fork

### Requirement: 平台差异处理

平台特定代码通过以下方式隔离：

1. **条件编译指令**（Taro 风格）：`#ifdef H5` `#ifdef MP-WEIXIN` `#endif`
2. **适配层模式**：H5 端通过 web/adapters/ 覆盖 Taro 组件
3. **运行时判断**（不推荐）：`process.env.TARO_ENV`

**Vite 端处理**：
- `#ifdef MP-WEIXIN ... #endif` 包裹的代码块在小程序端保留，H5 端**也保留**但需用 `typeof wx !== 'undefined'` 等守卫

**Taro 端处理**：
- 条件编译由 Taro babel 插件处理，正常编译

#### Scenario: 小程序 API 调用隔离

- **WHEN** 页面包含 `wx.setClipboardData({ data: content })`
- **THEN** H5 端 SHALL 用 navigator.clipboard 替代（适配层），不直接调用 wx API

#### Scenario: 条件编译指令

- **WHEN** 源码使用 `#ifdef MP-WEIXIN ... #endif`
- **THEN** Taro 端保留块内代码，H5 端 SHALL 保留块内代码但用 typeof wx 守卫避免运行时报错

### Requirement: 数据互通

两端数据通过后端 API 互通（未来实现）：

- H5 端使用 `fetch` / `axios`
- 小程序端使用 `wx.request` / Taro.request
- 统一封装在 `src/utils/apiClient.ts`

#### Scenario: API 调用统一入口

- **WHEN** 业务代码调用 `apiClient.get('/user/profile')`
- **THEN** H5 端 SHALL 走 fetch，小程序端走 wx.request，两端返回 Promise 一致

### Requirement: 独立部署

两端的产物**独立部署**：

- H5 端：Vercel / Netlify / 自建 nginx
- 小程序端：微信开发者工具上传到微信平台

#### Scenario: H5 端部署

- **WHEN** H5 产物构建完成
- **THEN** 应可部署到任意静态托管平台（Vercel/Netlify/nginx）

#### Scenario: 小程序端部署

- **WHEN** 小程序产物构建完成
- **THEN** 应可用微信开发者工具打开 dist/ 目录并上传发布