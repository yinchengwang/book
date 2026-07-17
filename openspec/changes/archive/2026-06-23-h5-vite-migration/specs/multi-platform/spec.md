# Capability: multi-platform

## Purpose

Reading Radar 2.0 同时支持 H5 网页端和微信小程序端，两端共享同一套业务代码、状态管理和数据模型。

## Requirements

### R1: 双端并行编译

H5 端和小程序端必须能**独立编译、独立运行**：

```bash
# H5 端（Vite）
cd web && npm run dev

# 小程序端（Taro）
npm run build:weapp
```

两套编译链**互不依赖**，可分别升级。

### R2: 共享源码

`src/` 下的业务代码必须同时被两端消费：

- H5 端通过 Vite alias 引用
- 小程序端通过 Taro 编译引用
- 同一份 `.tsx` 文件，两端都能识别

### R3: 平台差异处理

平台特定代码通过以下方式隔离：

1. **条件编译指令**（Taro 风格）：`#ifdef H5` `#ifdef MP-WEIXIN` `#endif`
2. **适配层模式**：H5 端通过 web/adapters/ 覆盖 Taro 组件
3. **运行时判断**（不推荐）：`process.env.TARO_ENV`

**Vite 端处理**：
- `#ifdef MP-WEIXIN ... #endif` 包裹的代码块在小程序端保留，H5 端**也保留**但需用 `typeof wx !== 'undefined'` 等守卫

**Taro 端处理**：
- 条件编译由 Taro babel 插件处理，正常编译

### R4: 数据互通

两端数据通过后端 API 互通（未来实现）：

- H5 端使用 `fetch` / `axios`
- 小程序端使用 `wx.request` / Taro.request
- 统一封装在 `src/utils/apiClient.ts`

### R5: 独立部署

两端的产物**独立部署**：

- H5 端：Vercel / Netlify / 自建 nginx
- 小程序端：微信开发者工具上传到微信平台

## Out of Scope

- 跨端组件库（暂时不抽离）
- 跨端数据加密（v2.0 不考虑）
- 跨端日志系统（v2.0 不考虑）
