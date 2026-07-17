## Why

在 SPA 导航下点击"学习"或"图解"选项卡时，目标页面渲染为空白。原因是 `swapPage()` 通过 fetch 获取页面内容并注入脚本，但页面的渲染入口（`DOMContentLoaded` 回调）只在首次加载时触发一次，SPA 切换后永不触发，导致 `render()` / `init()` 不执行。

## What Changes

- 修改 `data/app/nav-component.js` 中 `swapPage()` 方法：
  - 动态注入的外部 `<script>` 设置 `ns.async = false`，确保依赖脚本按序加载
  - 脚本全部注入后，手动派发 `new Event("DOMContentLoaded")`，触发目标页面的渲染回调

## Capabilities

### New Capabilities
- `spa-nav-render-fix`: 确保 SPA 导航切换后目标页面的初始化渲染函数被正确调用

### Modified Capabilities

（无）

## Impact

- 仅修改 `data/app/nav-component.js`（`swapPage()` 函数）
- 不影响 `grok.html`、`learn.html`、`items-registry.js`、`tech.js`、`static.js`、`learn-deep-bundle.js`
- 不改变现有功能的外部行为，所有页面在直接访问时仍然正常工作
