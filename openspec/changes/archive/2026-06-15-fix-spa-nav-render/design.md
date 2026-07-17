## Context

当前 `swapPage()` 将页面内 `<script>` 提取后重新注入 DOM。动态创建的 `<script>` 在浏览器中默认 **async**（加载完立即执行，不保证顺序）。注入的 inline 脚本同步执行并注册 `DOMContentLoaded` 回调，但该事件在首次加载后不会再次触发 → 目标页面的 `init()`/`render()` 永不执行，页面空白。

涉及页面：
- `grok.html`：依赖 `DOMContentLoaded` 调用 `render()`
- `learn.html`：依赖 `DOMContentLoaded` 调用 `init()`

## Goals / Non-Goals

**Goals:**
- SPA 导航到 `learn.html` / `grok.html` 时页面正常渲染
- 直接 URL 访问不受影响（向后兼容）
- 不修改 `learn.html` / `grok.html` 的渲染逻辑
- 外部队本加载时序正确（`tech.js` / `static.js` 在 inline 脚本之前执行完毕）

**Non-Goals:**
- 不引入构建工具或模块加载器
- 不改变 `index.html` 等正常运行的页面
- 不优化 8MB `learn-deep-bundle.js` 的加载性能（仅保证功能正确）

## Decisions

### 决策 1：在 swapPage 尾部手动派发 DOMContentLoaded

**选项：**
- A: `swapPage()` 注入所有脚本后 `document.dispatchEvent(new Event("DOMContentLoaded"))`
- B: 修改 `learn.html`/`grok.html` 暴露 `window.initFn`，由 `swapPage()` 直接调用
- C: 在页面中检查 `document.readyState !== "loading"` 时立即执行渲染

**选择 A**。原因：
- 零改动目标页面
- 对任何依赖 DOMContentLoaded 的页面自动生效（未来的新页面也一样）
- 语义清晰——这本质就是"内容已就绪，可以渲染了"

**问题**：仅 dispatch DOMContentLoaded 还不够——外部脚本（`tech.js`、`static.js`、`learn-deep-bundle.js`）默认 async 加载，dispatch 时它们可能还没加载完，`C_TECH_DATA`/`CATEGORIES` 未定义。需要配合决策 2。

### 决策 2：外部脚本顺序加载 + 等待完成

**选项：**
- A: 分离外部队本和 inline 脚本，外部队本设 `async = false` 并用 `onload` 链式等待，全部加载完后再注入 inline 脚本并 dispatch DOMContentLoaded
- B: 外部队本设 `async = false` 后立即 dispatch DOMContentLoaded（不等待）
- C: 使用 `document.write()` 强制同步加载

**选择 A**。原因：
- 不阻塞网络请求（浏览器并行 fetch），仅控制执行顺序
- 利用 `async = false` 保证按序执行，再用最后一个脚本的 `onload` 事件作为"全部就绪"信号
- 从缓存加载的脚本会同步执行 + 同步触发 onload，逻辑依然正确
- 相比 B，确保数据依赖可用；相比 C，不会破坏 DOM

### 决策 3：用计数器而非链式递归

**选项：**
- A: 计数器——所有外部队本共用 `loadedCount`，最后一个完成后触发
- B: 链式递归——每个脚本的 onload 调用下一个

**选择 A**。原因：
- 更简洁，不引入递归深度问题
- 利用 `async = false` 天然保证执行顺序，不需要链式控制
- 所有 fetch 可以并行（性能更好）

## Risks / Trade-offs

- [时序竞态] 若有页面在 `DOMContentLoaded` 回调中动态追加新脚本 → dispatch 在新脚本之前 → 不会触发。但当前所有页面都在静态 HTML 中声明脚本，不构成问题。
- [dispatch 顺序] 若 `swapPage()` 注入后还有后续操作（历史记录 pushState）修改了页面状态 → 用户可能在 DOMContentLoaded 回调中读到不一致的状态。解决：dispatch 放在 `swapPage()` 最后一行执行。
- [重复 dispatch] `swapPage()` 可能被多次调用（popstate / 点击导航），多次 dispatch DOMContentLoaded 会导致回调重复执行。解决：需要保证幂等——回调中通过状态变量（如 `rendered` guard）避免重复渲染。但这是页面自身的职责，`swapPage()` 不做额外保护。
