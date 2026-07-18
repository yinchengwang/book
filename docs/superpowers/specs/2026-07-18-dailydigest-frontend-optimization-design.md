# DailyDigest 前端优化设计文档

> 2026-07-18
> 项目：DailyDigest — 每日数据库 & AI 技术速览

## 概述

对 DailyDigest 前端进行三项核心优化：分页组件重构（含每页条数选择）、翻译功能集成、UI 卡片列表美化（时间线风格）。涉及时面：DailyView、SearchView、HistoryView、DetailView，以及 Pagination、ItemCard 组件。

---

## 1. 分页组件重构

### 1.1 页码标签

将当前「← 上一页 | 1/N | 下一页 →」样式，改为带数字页码标签的完整分页器。

**规则：**
- 显示当前页前后 2 页的数字按钮
- 首尾页（第 1 页和最后一页）始终可见
- 超出范围用 `...` 省略号占据位置
- 当前页使用品牌色实心按钮高亮
- 其余页码按钮使用 `btn-ghost` 样式

**示例（共 10 页，当前第 5 页）：**
```
← 上一页  1  2  3  4  5  6  7  ...  10  下一页 →
```

**示例（共 10 页，当前第 1 页）：**
```
← 上一页  1  2  3  ...  10  下一页 →
```

**边界条件：**
- 总页数 ≤ 7 时，全部显示，不出现省略号
- 当前页为第 1 页时，「上一页」禁用
- 当前页为最后一页时，「下一页」禁用

### 1.2 每页条数选择器

- 下拉选择器，选项：`10 / 20 / 50 / 100`
- 默认值：`10`
- 位置：分页栏最左侧
- 使用现有设计系统的 `.input` 类样式，紧凑尺寸
- 切换条数时，自动重置到第 1 页
- 条数选择器与页码标签在同一行，左右布局

**布局示意：**
```
[10条/页 ▼]  ← 上一页  1  2  3  ...  8  9  10  下一页 →   1/10
```

### 1.3 影响范围

| 组件/页面 | 改动 |
|-----------|------|
| `Pagination.vue` | 重构为带页码标签和条数选择器 |
| `DailyView.vue` | 传递 `pageSize` 改为响应式，监听变化重置 page=1 |
| `SearchView.vue` | 同上 |
| `HistoryView.vue` | 同上 |

---

## 2. 翻译功能

### 2.1 后端接口

**新增接口：** `POST /api/v1/translate`

**请求体：**
```json
{
  "text": "需要翻译的英文内容",
  "source_lang": "en",
  "target_lang": "zh"
}
```

**响应体：**
```json
{
  "translated_text": "翻译后的中文内容"
}
```

**实现逻辑：**
- 调用 LLM（与摘要生成共用 `LLM_API_KEY`）
- Prompt：`"请将以下英文技术内容翻译成中文，保持专业术语准确：\n\n{text}"`
- 如果 LLM 调用失败，返回原文

### 2.2 前端交互

**ItemCard.vue 修改：**
- 卡片底部新增「🌐 翻译」按钮（`btn-ghost` + `btn-sm` 样式）
- 点击后：
  1. 按钮变为加载旋转状态
  2. 调用 `POST /api/v1/translate`，传入标题和摘要拼接文本
  3. 翻译成功 → 替换卡片内标题和摘要显示
  4. 按钮文字变为「原文」
- 再次点击「原文」→ 恢复原始标题和摘要，按钮变回「翻译」
- 翻译内容存储在组件本地状态（`ref`），不持久化到后端

**API 层新增：**
```typescript
translate(text: string): Promise<{ translated_text: string }>
```

### 2.3 影响范围

| 组件/页面 | 改动 |
|-----------|------|
| `ItemCard.vue` | 新增翻译按钮、翻译状态、原文/译文切换 |
| `DetailView.vue` | 新增翻译按钮（标题+摘要+原文内容） |
| `api/client.ts` | 新增 `translate()` 方法 |
| 后端 `routers/` | 新增 `translate.py` 路由 |

---

## 3. UI 美化 — 时间线卡片列表

### 3.1 签名式元素：时间线

内容列表左侧增加一条垂直时间线，所有卡片悬挂在时间线上，形成"时间轴"视觉。

### 3.2 视觉结构

```
       │
  ──●──┌──────────────────────────────────────┐
       │ [arXiv]                       评分 88 │
       │                                        │
       │ Llama 3: 高效开源大语言模型             │
       │                                        │
       │ 这是一篇关于最新 Llama 3 模型的...      │
       │                                        │
       │ #transformer  #llm            今天     │
       │ [🌐 翻译]                             │
       └──────────────────────────────────────┘
       │
  ──●──┌──────────────────────────────────────┐
       │ [GitHub]                       评分 95 │
       │ ...                                    │
       └──────────────────────────────────────┘
       │
```

### 3.3 具体改动

| 属性 | 值 | 说明 |
|------|-----|------|
| 卡片间距 | `gap: var(--space-5)` (20px) | 从 12px 加大 |
| 时间线竖线 | 1px 虚线 `var(--color-border-light)` | 贯穿列表全部高度 |
| 节点圆点 | 8px 实心圆 `var(--color-source-*)` | 卡片左侧，颜色随来源变化 |
| 左边框 | 保留 4px 来源色实线 | 已有样式，保持不变 |
| 底部标尺线 | `1px` 来源色 `color-mix(8%)` | 每条卡片底部 |
| hover 左边框 | 6px 来源色 | 加粗动画 |
| hover 阴影 | `--shadow-card-hover` 扩散 | 已有 |
| hover 上移 | `-2px` | 从 -1px 加强 |

### 3.4 CSS 实现方案

时间线竖线通过 `.item-list` 容器的伪元素实现：
```css
.item-list {
  position: relative;
  gap: var(--space-5);
}
.item-list::before {
  content: '';
  position: absolute;
  left: 19px;          /* 对齐节点圆点中心 */
  top: 0;
  bottom: 0;
  width: 1px;
  border-left: 1px dashed var(--color-border-light);
}
```

节点圆点通过 ItemCard 的伪元素实现：
```css
.item-card::before {
  content: '';
  position: absolute;
  left: -28px;         /* 从卡片左侧伸出到时间线位置 */
  top: 24px;           /* 对齐卡片头部 */
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--source-color);  /* 动态设置 */
}
```

### 3.5 影响范围

| 组件/页面 | 改动 |
|-----------|------|
| `ItemCard.vue` | 新增节点圆点伪元素、底部标尺线、hover 增强 |
| `DailyView.vue` | `.item-list` 新增时间线竖线，间距改为 20px |
| `SearchView.vue` | 同上 |
| `HistoryView.vue` | 同上 |
| `themes.css` | 可选新增 `--space-5` 确保定义（已有） |

---

## 4. 兼容性说明

- 所有颜色使用 CSS 变量，四色主题（light/dark/eye/warm）自动适配
- 不引入新的 UI 依赖库
- 新组件保持与现有设计系统一致（字号、圆角、阴影 Token）
- 翻译功能需后端配置 `LLM_API_KEY`，无 key 时按钮隐藏或提示
- 每页条数选择器默认 10，可通过 `localStorage` 持久化用户偏好

---

## 5. 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `web/src/components/Pagination.vue` | 重写 | 页码标签 + 每页条数选择器 |
| `web/src/components/ItemCard.vue` | 修改 | 翻译按钮 + 节点圆点 + 底部标尺线 + hover 增强 |
| `web/src/pages/DailyView.vue` | 修改 | pageSize 改为响应式，时间线竖线样式 |
| `web/src/pages/SearchView.vue` | 修改 | 同上 |
| `web/src/pages/HistoryView.vue` | 修改 | 同上 |
| `web/src/pages/DetailView.vue` | 修改 | 新增翻译按钮 |
| `web/src/api/client.ts` | 修改 | 新增 `translate()` 方法 |
| `backend/app/routers/translate.py` | 新增 | 翻译接口路由 |
| `backend/app/main.py` | 修改 | 注册翻译路由 |