## 1. 修改 swapPage 脚本注入逻辑

- [x] 1.1 在 `data/app/nav-component.js` 的 `swapPage()` 中，将 `scriptsToRun` 拆分为外部脚本（有 src）和 inline 脚本（无 src）
- [x] 1.2 外部脚本注入时设置 `ns.async = false` 并用 `onload`/`onerror` 计数器跟踪全部加载完成
- [x] 1.3 全部外部脚本加载完毕后，再注入 inline 脚本，然后 `document.dispatchEvent(new Event("DOMContentLoaded"))`

## 2. 处理边界情况

- [x] 2.1 当页面没有外部脚本时（只有 inline 脚本），直接执行 inline 脚本并 dispatch DOMContentLoaded
- [x] 2.2 确保 `swapPage()` 尾部 dispatch 的 DOMContentLoaded 不破坏 popstate 导航和 pushState 逻辑

## 3. 验证

- [x] 3.1 用 `node server.js` 启动本地服务，验证 SPA 导航到 `learn.html` 正常渲染
- [x] 3.2 验证 SPA 导航到 `grok.html` 正常渲染
- [x] 3.3 验证直接 URL 访问 `learn.html` 和 `grok.html` 仍然正常工作
- [x] 3.4 验证多次 SPA 导航切换（首页 → 学习 → 首页 → 图解）不会产生重复渲染或空白页
