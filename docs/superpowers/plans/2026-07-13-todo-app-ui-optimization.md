# Todo App UI 优化实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 全面优化 Todo App 前端 UI/UX，包含视觉效果、布局、交互体验三大方面

**Architecture:** 基于现有 Vue 3 + Vite 项目，扩展 CSS 变量系统，添加动画层，优化组件交互

**Tech Stack:** Vue 3 Composition API, CSS Variables, CSS Animations, 无新增依赖

---

## 文件结构

```
engineering/apps/web/todo-app/src/
├── styles/
│   └── main.css          # 扩展 CSS 变量和动画
├── components/
│   ├── TodoCard.vue      # 添加快捷操作、优先级色条
│   ├── DetailPanel.vue   # 滑入动画
│   ├── FilterBar.vue     # 搜索防抖
│   └── EmptyState.vue    # 新增空状态组件
├── views/
│   ├── ListView.vue      # 列表动画
│   ├── BoardView.vue     # 看板优化
│   └── StatsView.vue     # 统计图表优化
└── App.vue               # 添加键盘快捷键
```

---

## Task 1: 扩展 CSS 变量和动画系统

**Files:**
- Modify: `engineering/apps/web/todo-app/src/styles/main.css`

- [ ] **Step 1: 扩展 CSS 变量**

在 `:root` 块末尾添加新变量：

```css
/* 过渡动画 */
--transition-fast: 100ms;
--transition-normal: 200ms;
--transition-slow: 300ms;

/* 阴影层次 */
--shadow-sm: 0 2px 8px rgba(0,0,0,0.2);
--shadow-md: 0 4px 16px rgba(0,0,0,0.3);
--shadow-lg: 0 8px 32px rgba(0,0,0,0.4);

/* 发光效果 */
--glow-primary: 0 0 20px rgba(47, 129, 247, 0.3);
--glow-success: 0 0 20px rgba(63, 185, 80, 0.3);
```

- [ ] **Step 2: 添加动画关键帧**

在文件末尾添加：

```css
/* 页面过渡 */
@keyframes fadeIn {
  from { opacity: 0; transform: translateY(8px); }
  to { opacity: 1; transform: translateY(0); }
}

@keyframes slideInRight {
  from { opacity: 0; transform: translateX(20px); }
  to { opacity: 1; transform: translateX(0); }
}

@keyframes scaleIn {
  from { opacity: 0; transform: scale(0.95); }
  to { opacity: 1; transform: scale(1); }
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

/* 列表项交错动画 */
.todo-list-enter-active { animation: fadeIn var(--transition-normal) ease-out; }
.todo-list-leave-active { animation: fadeIn var(--transition-fast) ease-in reverse; }
.todo-list-move { transition: transform var(--transition-normal) ease; }
```

- [ ] **Step 3: 添加通用动画类**

```css
.animate-fade-in { animation: fadeIn var(--transition-normal) ease-out; }
.animate-slide-in { animation: slideInRight var(--transition-normal) ease-out; }
.animate-scale-in { animation: scaleIn var(--transition-normal) ease-out; }
```

- [ ] **Step 4: 提交**

```bash
git add engineering/apps/web/todo-app/src/styles/main.css
git commit -m "style: 扩展 CSS 变量和动画系统"
```

---

## Task 2: 优化 TodoCard 组件

**Files:**
- Modify: `engineering/apps/web/todo-app/src/components/TodoCard.vue`

- [ ] **Step 1: 添加优先级色条和悬浮效果**

替换 `<style scoped>` 部分：

```css
<style scoped>
.todo-card {
  cursor: pointer;
  transition: transform var(--transition-fast), box-shadow var(--transition-fast), border-color var(--transition-fast);
  position: relative;
  padding-left: 20px;
}

.todo-card::before {
  content: '';
  position: absolute;
  left: 0;
  top: 0;
  bottom: 0;
  width: 4px;
  border-radius: var(--radius) 0 0 var(--radius);
}

.todo-card.priority-0::before { background: var(--danger); }
.todo-card.priority-1::before { background: var(--warning); }
.todo-card.priority-2::before { background: var(--primary); }
.todo-card.priority-3::before { background: var(--success); }
.todo-card.priority-4::before { background: var(--text-muted); }

.todo-card:hover {
  transform: translateY(-2px);
  box-shadow: var(--glow-primary);
  border-color: var(--primary);
}

.todo-card.is-closed { opacity: 0.6; }
.todo-card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
.priority-badge { font-size: 12px; }
.todo-id { color: var(--text-muted); font-size: 12px; }
.todo-title { font-weight: 600; margin-bottom: 8px; word-break: break-word; }
.is-closed .todo-title { text-decoration: line-through; color: var(--text-muted); }
.todo-meta { display: flex; gap: 6px; align-items: center; flex-wrap: wrap; margin-bottom: 8px; }
.due-date { font-size: 12px; color: var(--text-muted); }
.due-date.overdue { color: var(--danger); }
.todo-progress { display: flex; align-items: center; gap: 8px; font-size: 12px; color: var(--text-muted); }
.progress-bar { flex: 1; height: 4px; background: var(--bg); border-radius: 2px; overflow: hidden; }
.progress-bar-fill { height: 100%; background: var(--success); transition: width var(--transition-normal); }

/* 快捷操作按钮 */
.todo-actions {
  position: absolute;
  top: 8px;
  right: 8px;
  display: flex;
  gap: 4px;
  opacity: 0;
  transition: opacity var(--transition-fast);
}
.todo-card:hover .todo-actions { opacity: 1; }
.action-btn {
  width: 24px;
  height: 24px;
  border-radius: 4px;
  border: none;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 12px;
  transition: background var(--transition-fast);
}
.action-btn.done { background: var(--success); color: white; }
.action-btn.delete { background: var(--danger); color: white; }
</style>
```

- [ ] **Step 2: 添加快捷操作按钮**

在模板中添加操作按钮：

```html
<div class="todo-actions">
  <button class="action-btn done" @click.stop="$emit('toggle')" title="完成">✓</button>
  <button class="action-btn delete" @click.stop="$emit('delete')" title="删除">×</button>
</div>
```

- [ ] **Step 3: 添加 progress-bar 颜色变化**

修改 `progress-bar-fill` 样式根据进度显示不同颜色：

```css
.progress-bar-fill {
  height: 100%;
  transition: width var(--transition-normal);
  background: var(--success);
}
.progress-bar-fill[style*="100"] { background: var(--success); }
.progress-bar-fill[style*="0"] { background: var(--text-muted); }
```

- [ ] **Step 4: 提交**

```bash
git add engineering/apps/web/todo-app/src/components/TodoCard.vue
git commit -m "feat: 优化 TodoCard 优先级色条和悬浮效果"
```

---

## Task 3: 创建 EmptyState 组件

**Files:**
- Create: `engineering/apps/web/todo-app/src/components/EmptyState.vue`

- [ ] **Step 1: 创建组件**

```vue
<template>
  <div class="empty-state">
    <div class="empty-icon">{{ icon }}</div>
    <h3 class="empty-title">{{ title }}</h3>
    <p class="empty-desc">{{ description }}</p>
    <button v-if="actionText" class="btn btn-primary" @click="$emit('action')">
      {{ actionText }}
    </button>
  </div>
</template>

<script setup>
defineProps({
  icon: { type: String, default: '📋' },
  title: { type: String, default: '暂无数据' },
  description: { type: String, default: '' },
  actionText: { type: String, default: '' }
})
defineEmits(['action'])
</script>

<style scoped>
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 48px 24px;
  text-align: center;
  animation: fadeIn var(--transition-normal) ease-out;
}
.empty-icon { font-size: 48px; margin-bottom: 16px; opacity: 0.8; }
.empty-title { font-size: 18px; font-weight: 600; margin-bottom: 8px; color: var(--text); }
.empty-desc { font-size: 14px; color: var(--text-muted); margin-bottom: 20px; max-width: 300px; }
</style>
```

- [ ] **Step 2: 提交**

```bash
git add engineering/apps/web/todo-app/src/components/EmptyState.vue
git commit -m "feat: 创建 EmptyState 空状态组件"
```

---

## Task 4: 优化 FilterBar 组件

**Files:**
- Modify: `engineering/apps/web/todo-app/src/components/FilterBar.vue`

- [ ] **Step 1: 添加搜索防抖**

在 `<script setup>` 中添加防抖逻辑：

```javascript
import { ref, watch } from 'vue'

const searchQuery = ref('')
const emit = defineEmits(['filter', 'new'])
let searchTimer = null

watch(searchQuery, (val) => {
  clearTimeout(searchTimer)
  searchTimer = setTimeout(() => {
    emit('filter', { search: val })
  }, 300)
})
```

- [ ] **Step 2: 优化样式**

在 `<style scoped>` 中添加：

```css
.filter-bar {
  display: flex;
  gap: 12px;
  padding: 12px 16px;
  background: var(--bg-elev);
  border-bottom: 1px solid var(--border);
  align-items: center;
}
.search-input-wrapper {
  position: relative;
  flex: 1;
  max-width: 300px;
}
.search-icon {
  position: absolute;
  left: 10px;
  top: 50%;
  transform: translateY(-50%);
  color: var(--text-muted);
}
.search-input {
  padding-left: 32px;
}
.filters {
  display: flex;
  gap: 8px;
  flex: 1;
}
```

- [ ] **Step 3: 提交**

```bash
git add engineering/apps/web/todo-app/src/components/FilterBar.vue
git commit -m "feat: FilterBar 添加搜索防抖和优化样式"
```

---

## Task 5: 优化 DetailPanel 组件

**Files:**
- Modify: `engineering/apps/web/todo-app/src/components/DetailPanel.vue`

- [ ] **Step 1: 添加滑入动画**

修改 `<style scoped>` 部分：

```css
<style scoped>
.detail-panel {
  padding: 24px;
  overflow-y: auto;
  animation: slideInRight var(--transition-normal) ease-out;
  background: var(--bg-elev);
  height: 100%;
}

.detail-header {
  margin-bottom: 16px;
  padding-bottom: 16px;
  border-bottom: 1px solid var(--border);
}
.detail-header h2 { margin: 8px 0; font-size: 20px; }
.todo-id { color: var(--text-muted); font-size: 12px; }
.description {
  background: var(--bg-base);
  padding: 12px;
  border-radius: var(--radius);
  white-space: pre-wrap;
  margin-bottom: 12px;
  border: 1px solid var(--border);
}
.empty-desc { color: var(--text-muted); font-style: italic; }
.todo-meta { display: flex; gap: 6px; align-items: center; margin-bottom: 16px; flex-wrap: wrap; }
.detail-actions { margin-top: 16px; padding-top: 16px; border-top: 1px solid var(--border); }

/* 关闭按钮 */
.close-btn {
  position: absolute;
  top: 16px;
  right: 16px;
  width: 32px;
  height: 32px;
  border-radius: 50%;
  border: none;
  background: var(--bg-elev2);
  color: var(--text-muted);
  cursor: pointer;
  font-size: 18px;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: background var(--transition-fast), color var(--transition-fast);
}
.close-btn:hover { background: var(--danger); color: white; }
.detail-panel { position: relative; padding-right: 48px; }
</style>
```

- [ ] **Step 2: 添加关闭按钮和外部点击关闭**

```html
<template>
  <div v-if="todo" class="detail-panel">
    <button class="close-btn" @click="$emit('close')">×</button>
    <!-- ... 其余内容保持不变 ... -->
  </div>
</template>
```

- [ ] **Step 3: 提交**

```bash
git add engineering/apps/web/todo-app/src/components/DetailPanel.vue
git commit -m "feat: DetailPanel 添加滑入动画和关闭按钮"
```

---

## Task 6: 优化 ListView 视图

**Files:**
- Modify: `engineering/apps/web/todo-app/src/views/ListView.vue`

- [ ] **Step 1: 添加列表动画**

在模板中包裹 TodoCard：

```html
<TransitionGroup name="todo-list" tag="div" class="todo-list">
  <TodoCard v-for="todo in todos" :key="todo.id" :todo="todo" @click="select(todo)" />
</TransitionGroup>
```

- [ ] **Step 2: 使用 EmptyState**

```html
<EmptyState
  v-if="todos.length === 0"
  icon="📋"
  title="还没有待办"
  description="点击右上角按钮创建你的第一个待办事项"
  actionText="创建待办"
  @action="showCreate = true"
/>
```

- [ ] **Step 3: 添加骨架屏**

创建 `SkeletonLoader.vue` 并在 ListView 中使用：

```vue
<template>
  <div class="skeleton-list">
    <div v-for="i in count" :key="i" class="skeleton-card">
      <div class="skeleton-line short"></div>
      <div class="skeleton-line"></div>
      <div class="skeleton-line medium"></div>
    </div>
  </div>
</template>

<script setup>
defineProps({ count: { type: Number, default: 3 } })
</script>

<style scoped>
.skeleton-card {
  background: var(--bg-elev);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 16px;
  margin-bottom: 8px;
}
.skeleton-line {
  height: 12px;
  background: linear-gradient(90deg, var(--bg-elev2) 25%, var(--border) 50%, var(--bg-elev2) 75%);
  background-size: 200% 100%;
  animation: shimmer 1.5s infinite;
  border-radius: 4px;
  margin-bottom: 8px;
}
.skeleton-line.short { width: 30%; }
.skeleton-line.medium { width: 60%; }
@keyframes shimmer {
  0% { background-position: 200% 0; }
  100% { background-position: -200% 0; }
}
</style>
```

- [ ] **Step 4: 在 ListView 中添加加载状态**

```javascript
const loading = ref(true)
watchEffect(() => { loading.value = todos.value.length === 0 })
```

- [ ] **Step 5: 提交**

```bash
git add engineering/apps/web/todo-app/src/views/ListView.vue
git add engineering/apps/web/todo-app/src/components/SkeletonLoader.vue
git commit -m "feat: ListView 添加列表动画和骨架屏"
```

---

## Task 7: 优化 BoardView 视图

**Files:**
- Modify: `engineering/apps/web/todo-app/src/views/BoardView.vue`

- [ ] **Step 1: 优化看板列样式**

```css
<style scoped>
.board-view { height: 100vh; display: flex; flex-direction: column; }
.board-controls {
  display: flex;
  gap: 8px;
  padding: 12px 24px;
  background: var(--bg-elev);
  border-bottom: 1px solid var(--border);
  align-items: center;
}
.board-columns {
  display: flex;
  gap: 16px;
  padding: 16px;
  overflow-x: auto;
  flex: 1;
}
.board-column {
  min-width: 280px;
  flex: 1;
  max-width: 320px;
  background: var(--bg-elev);
  border-radius: var(--radius);
  padding: 12px;
  border: 1px solid var(--border);
}
.column-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 12px;
  padding-bottom: 8px;
  border-bottom: 2px solid var(--border);
}
.column-count {
  color: var(--text-muted);
  font-weight: normal;
  font-size: 12px;
  background: var(--bg-elev2);
  padding: 2px 8px;
  border-radius: 10px;
}
.column-todos { display: flex; flex-direction: column; gap: 8px; }
.column-empty {
  text-align: center;
  color: var(--text-muted);
  font-size: 12px;
  padding: 20px 0;
  border: 2px dashed var(--border);
  border-radius: var(--radius);
}
</style>
```

- [ ] **Step 2: 提交**

```bash
git add engineering/apps/web/todo-app/src/views/BoardView.vue
git commit -m "feat: BoardView 优化看板列样式"
```

---

## Task 8: 添加快捷键支持

**Files:**
- Modify: `engineering/apps/web/todo-app/src/App.vue`

- [ ] **Step 1: 添加键盘事件监听**

```javascript
import { onMounted, onUnmounted, ref } from 'vue'
import { useRouter } from 'vue-router'

const router = useRouter()
const showCreate = ref(false)

function handleKeydown(e) {
  // 忽略输入框内的按键
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
    if (e.key === 'Escape') e.target.blur()
    return
  }

  switch (e.key.toLowerCase()) {
    case 'n':
      if (!showCreate.value) {
        showCreate.value = true
      }
      break
    case '/':
      // 聚焦搜索框
      const searchInput = document.querySelector('.search-input')
      if (searchInput) searchInput.focus()
      break
    case 'Escape':
      showCreate.value = false
      break
  }
}

onMounted(() => { document.addEventListener('keydown', handleKeydown) })
onUnmounted(() => { document.removeEventListener('keydown', handleKeydown) })
```

- [ ] **Step 2: 在 App.vue 中传递 showCreate**

修改 App.vue 模板传递 `showCreate` 给子组件：

```html
<router-view v-slot="{ Component }">
  <component :is="Component" v-model:showCreate="showCreate" />
</router-view>
```

- [ ] **Step 3: 提交**

```bash
git add engineering/apps/web/todo-app/src/App.vue
git commit -m "feat: 添加全局快捷键支持 (N 新建, / 搜索, Esc 关闭)"
```

---

## Task 9: 响应式布局优化

**Files:**
- Modify: `engineering/apps/web/todo-app/src/styles/main.css`

- [ ] **Step 1: 添加响应式媒体查询**

```css
/* 移动端适配 */
@media (max-width: 768px) {
  .nav {
    position: fixed;
    bottom: 0;
    left: 0;
    right: 0;
    top: auto;
    justify-content: space-around;
    padding: 8px 0;
    z-index: 100;
    border-top: 1px solid var(--border);
    border-bottom: none;
  }
  .nav-link {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
    font-size: 11px;
    padding: 4px 8px;
  }

  .list-view { padding-bottom: 60px; }
  .list-body { grid-template-columns: 1fr !important; }
  .board-columns { flex-direction: column; }
  .board-column { max-width: 100%; }
  .stats-grid { grid-template-columns: repeat(2, 1fr) !important; }
}

/* 平板端 */
@media (min-width: 769px) and (max-width: 1024px) {
  .list-body { grid-template-columns: 300px 1fr; }
}
```

- [ ] **Step 2: 提交**

```bash
git add engineering/apps/web/todo-app/src/styles/main.css
git commit -m "feat: 添加响应式布局适配"
```

---

## Task 10: Toast 通知优化

**Files:**
- Modify: `engineering/apps/web/todo-app/src/App.vue`

- [ ] **Step 1: 优化 Toast 样式**

```css
.toast {
  position: fixed;
  bottom: 80px;
  right: 16px;
  padding: 12px 20px;
  border-radius: var(--radius);
  background: var(--bg-elev);
  border: 1px solid var(--border);
  color: var(--text);
  font-size: 14px;
  z-index: 1000;
  animation: slideInRight var(--transition-normal) ease-out;
  box-shadow: var(--shadow-lg);
  max-width: 300px;
}
.toast-success { border-color: var(--success); background: rgba(63, 185, 80, 0.1); }
.toast-error { border-color: var(--danger); background: rgba(248, 81, 73, 0.1); }
.toast-info { border-color: var(--primary); background: rgba(47, 129, 247, 0.1); }
@keyframes slideInRight {
  from { transform: translateX(100%); opacity: 0; }
  to { transform: translateX(0); opacity: 1; }
}
```

- [ ] **Step 2: 提交**

```bash
git add engineering/apps/web/todo-app/src/App.vue
git commit -m "style: 优化 Toast 通知样式和位置"
```

---

## 验收检查

- [ ] 所有交互有视觉反馈（hover、点击、加载）
- [ ] 动画流畅（60fps）
- [ ] 移动端布局正常
- [ ] 快捷键工作正常
- [ ] 空状态友好提示
- [ ] 代码提交完整
