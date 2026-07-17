#### 9.1 项目脚手架

- [ ] **Step 9.1.1：创建目录结构**

```bash
mkdir -p engineering/apps/web/todo-app/src/components
mkdir -p engineering/apps/web/todo-app/src/router
mkdir -p engineering/apps/web/todo-app/src/styles
mkdir -p engineering/apps/web/todo-app/src/views
mkdir -p engineering/apps/web/todo-app/public
```

- [ ] **Step 9.1.2：创建 package.json**

```json
{
  "name": "todo-app",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "vite build",
    "preview": "vite preview"
  },
  "dependencies": {
    "vue": "^3.4.0",
    "vue-router": "^4.2.0"
  },
  "devDependencies": {
    "@vitejs/plugin-vue": "^5.0.0",
    "vite": "^5.0.0"
  }
}
```

写入：`engineering/apps/web/todo-app/package.json`

- [ ] **Step 9.1.3：创建 vite.config.js**

```js
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  base: './',
  build: {
    outDir: 'dist',
    assetsDir: 'assets'
  },
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://localhost:8080',
        changeOrigin: true
      }
    }
  }
})
```

写入：`engineering/apps/web/todo-app/vite.config.js`

- [ ] **Step 9.1.4：创建 index.html**

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Todo App</title>
</head>
<body>
  <div id="app"></div>
  <script type="module" src="/src/main.js"></script>
</body>
</html>
```

写入：`engineering/apps/web/todo-app/index.html`

- [ ] **Step 9.1.5：创建 src/main.js**

```js
import { createApp } from 'vue'
import App from './App.vue'
import router from './router/index.js'
import './styles/main.css'

createApp(App).use(router).mount('#app')
```

写入：`engineering/apps/web/todo-app/src/main.js`

- [ ] **Step 9.1.6：创建 src/router/index.js**

```js
import { createRouter, createWebHistory } from 'vue-router'
import ListView from '../views/ListView.vue'
import BoardView from '../views/BoardView.vue'
import StatsView from '../views/StatsView.vue'
import GroupManager from '../views/GroupManager.vue'

const routes = [
  { path: '/', component: ListView, meta: { title: '待办列表' } },
  { path: '/board', component: BoardView, meta: { title: '看板视图' } },
  { path: '/stats', component: StatsView, meta: { title: '统计看板' } },
  { path: '/groups', component: GroupManager, meta: { title: '分组管理' } }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

router.beforeEach((to) => {
  document.title = to.meta.title ? `${to.meta.title} - Todo App` : 'Todo App'
})

export default router
```

写入：`engineering/apps/web/todo-app/src/router/index.js`

- [ ] **Step 9.1.7：创建 src/styles/main.css**

```css
/* ============================================================
 * 全局样式（深色主题）
 * ============================================================ */
:root {
  --bg: #0d1117;
  --bg-elev: #161b22;
  --bg-elev2: #1c2128;
  --border: #30363d;
  --border-strong: #484f58;
  --text: #e6edf3;
  --text-muted: #7d8590;
  --primary: #2f81f7;
  --primary-hover: #4a92f8;
  --success: #3fb950;
  --danger: #f85149;
  --warning: #d29922;
  --purple: #a371f7;
  --label: rgba(56, 139, 253, 0.15);
  --label-text: #79c0ff;
  --closed: #8957e5;
  --radius: 6px;
  --radius-sm: 4px;
  --shadow: 0 8px 24px rgba(1, 4, 9, 0.5);
}

* { box-sizing: border-box; margin: 0; padding: 0; }

body {
  background: var(--bg);
  color: var(--text);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Microsoft YaHei", sans-serif;
  font-size: 14px;
  line-height: 1.5;
  min-height: 100vh;
}

/* 导航 */
.nav { display: flex; gap: 8px; padding: 12px 24px; background: var(--bg-elev); border-bottom: 1px solid var(--border); }
.nav-link { color: var(--text-muted); text-decoration: none; padding: 6px 12px; border-radius: var(--radius-sm); font-size: 13px; }
.nav-link:hover { background: var(--bg-elev2); color: var(--text); }
.nav-link.router-link-active { background: var(--primary); color: #fff; }

/* 按钮 */
.btn { border: 1px solid transparent; padding: 6px 14px; border-radius: var(--radius-sm); font-size: 13px; font-weight: 500; cursor: pointer; transition: background 0.15s; }
.btn:disabled { opacity: 0.5; cursor: not-allowed; }
.btn-primary { background: var(--success); color: #fff; }
.btn-primary:hover:not(:disabled) { background: #46c260; }
.btn-secondary { background: var(--bg-elev2); color: var(--text); border-color: var(--border); }
.btn-secondary:hover:not(:disabled) { background: #2d333b; }
.btn-danger { background: transparent; color: var(--danger); border-color: rgba(248,81,73,0.4); }
.btn-sm { padding: 4px 10px; font-size: 12px; }

/* 表单 */
.form-input, .form-textarea, .form-select {
  width: 100%; background: var(--bg); border: 1px solid var(--border);
  color: var(--text); padding: 8px 10px; border-radius: var(--radius-sm);
  font-size: 13px; outline: none; font-family: inherit;
}
.form-input:focus, .form-textarea:focus { border-color: var(--primary); }
.form-textarea { resize: vertical; min-height: 80px; }
.label-text { display: block; font-size: 13px; font-weight: 600; margin: 12px 0 6px; }

/* 卡片 */
.card { background: var(--bg-elev); border: 1px solid var(--border); border-radius: var(--radius); padding: 16px; }
.card:hover { border-color: var(--border-strong); }

/* 标签 */
.label { display: inline-block; padding: 0 8px; font-size: 12px; line-height: 20px; border-radius: 10px; background: var(--label); color: var(--label-text); font-weight: 500; }

/* 弹窗 */
.modal-mask { position: fixed; inset: 0; background: rgba(0,0,0,0.5); display: flex; align-items: center; justify-content: center; z-index: 100; }
.modal { background: var(--bg-elev); border: 1px solid var(--border); border-radius: var(--radius); padding: 24px; width: 480px; max-width: 90vw; box-shadow: var(--shadow); }
.modal h3 { margin: 0 0 16px; font-size: 16px; }

/* Toast */
.toast { position: fixed; top: 16px; right: 16px; padding: 10px 16px; border-radius: var(--radius); background: var(--bg-elev); border: 1px solid var(--border); color: var(--text); font-size: 13px; z-index: 1000; animation: slideIn 0.2s; }
.toast-success { border-color: var(--success); }
.toast-error { border-color: var(--danger); }
@keyframes slideIn { from { transform: translateX(20px); opacity: 0; } to { transform: translateX(0); opacity: 1; } }

/* 滚动条 */
::-webkit-scrollbar { width: 8px; height: 8px; }
::-webkit-scrollbar-track { background: transparent; }
::-webkit-scrollbar-thumb { background: var(--border); border-radius: 4px; }
::-webkit-scrollbar-thumb:hover { background: var(--border-strong); }

/* 优先级颜色 */
.priority-0 { color: #f85149; } /* 紧急 */
.priority-1 { color: #d29922; } /* 高 */
.priority-2 { color: #2f81f7; } /* 中 */
.priority-3 { color: #7ee787; } /* 低 */
.priority-4 { color: var(--text-muted); } /* 无 */

/* 过期 */
.overdue { color: var(--danger); font-weight: 600; }
```

写入：`engineering/apps/web/todo-app/src/styles/main.css`

- [ ] **Step 9.1.8：创建 src/api.js**

```js
const BASE = '/api'

const api = {
  /* Todo CRUD */
  list: (params) => fetch(`${BASE}/todos?` + new URLSearchParams(params)).then(r => r.json()),
  get: (id) => fetch(`${BASE}/todos/${id}`).then(r => r.json()),
  create: (body) => fetch(`${BASE}/todos`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  update: (id, body) => fetch(`${BASE}/todos/${id}`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  remove: (id) => fetch(`${BASE}/todos/${id}`, { method: 'DELETE' }).then(r => r.json()),
  updateSort: (id, sort_order) => fetch(`${BASE}/todos/${id}/sort`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ sort_order }) }).then(r => r.json()),

  /* 统计 */
  stats: () => fetch(`${BASE}/todos/stats`).then(r => r.json()),

  /* Checklist */
  addChecklist: (id, text) => fetch(`${BASE}/todos/${id}/checklist`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ text }) }).then(r => r.json()),
  toggleChecklist: (id, itemId) => fetch(`${BASE}/todos/${id}/checklist/${itemId}`, { method: 'PATCH' }).then(r => r.json()),
  removeChecklist: (id, itemId) => fetch(`${BASE}/todos/${id}/checklist/${itemId}`, { method: 'DELETE' }).then(r => r.json()),

  /* 评论 */
  listComments: (id) => fetch(`${BASE}/todos/${id}/comments`).then(r => r.json()),
  addComment: (id, text) => fetch(`${BASE}/todos/${id}/comments`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ text }) }).then(r => r.json()),
  deleteComment: (id, cid) => fetch(`${BASE}/todos/${id}/comments/${cid}`, { method: 'DELETE' }).then(r => r.json()),

  /* 分组 */
  listGroups: () => fetch(`${BASE}/groups`).then(r => r.json()),
  createGroup: (body) => fetch(`${BASE}/groups`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  updateGroup: (id, body) => fetch(`${BASE}/groups/${id}`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  deleteGroup: (id) => fetch(`${BASE}/groups/${id}`, { method: 'DELETE' }).then(r => r.json()),

  /* OPSX 变更 */
  createChange: (id) => fetch(`${BASE}/todos/${id}/create-change`, { method: 'POST' }).then(r => r.json()),
}

export default api
```

写入：`engineering/apps/web/todo-app/src/api.js`

#### 9.2 核心组件

- [ ] **Step 9.2.1：创建 src/App.vue**

```vue
<template>
  <div id="app">
    <nav class="nav">
      <router-link to="/" class="nav-link">📋 列表</router-link>
      <router-link to="/board" class="nav-link">📊 看板</router-link>
      <router-link to="/stats" class="nav-link">📈 统计</router-link>
      <router-link to="/groups" class="nav-link">🏷️ 分组</router-link>
    </nav>
    <router-view />
    <div v-if="toast" :class="['toast', 'toast-' + toast.type]">{{ toast.msg }}</div>
  </div>
</template>

<script setup>
import { ref, provide } from 'vue'

const toast = ref(null)

function showToast(msg, type = 'success') {
  toast.value = { msg, type }
  setTimeout(() => { toast.value = null }, 2200)
}

provide('showToast', showToast)
</script>
```

写入：`engineering/apps/web/todo-app/src/App.vue`

- [ ] **Step 9.2.2：创建 src/components/TodoCard.vue**

```vue
<template>
  <div :class="['todo-card', 'card', todo.status === 'closed' ? 'is-closed' : '']" @click="$emit('click')">
    <div class="todo-card-header">
      <span :class="['priority-badge', 'priority-' + todo.priority]">
        {{ priorityLabel }}
      </span>
      <span class="todo-id">#{{ todo.id }}</span>
    </div>
    <div class="todo-title">{{ todo.title }}</div>
    <div class="todo-meta">
      <span v-if="todo.due_date && todo.due_date > 0" :class="['due-date', isOverdue ? 'overdue' : '']">
        📅 {{ formatDate(todo.due_date) }}
      </span>
      <span v-for="label in labels" :key="label" class="label">{{ label }}</span>
    </div>
    <div v-if="todo.checklist && todo.checklist.length" class="todo-progress">
      <span>{{ doneCount }}/{{ todo.checklist.length }}</span>
      <div class="progress-bar">
        <div class="progress-bar-fill" :style="{ width: progressPct + '%' }"></div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({ todo: Object })
defineEmits(['click'])

const PRIORITY_LABELS = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']
const priorityLabel = computed(() => PRIORITY_LABELS[props.todo.priority] || '⚪无')

const labels = computed(() => {
  if (!props.todo.labels) return []
  if (Array.isArray(props.todo.labels)) return props.todo.labels
  try { return JSON.parse(props.todo.labels) } catch { return [] }
})

const doneCount = computed(() =>
  (props.todo.checklist || []).filter(i => i.done).length
)
const progressPct = computed(() =>
  props.todo.checklist?.length ? Math.round(doneCount.value * 100 / props.todo.checklist.length) : 0
)

const now = Date.now() / 1000
const isOverdue = computed(() =>
  props.todo.due_date > 0 && props.todo.due_date < now && props.todo.status === 'open'
)

function formatDate(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getMonth()+1}/${d.getDate()}`
}
</script>

<style scoped>
.todo-card { cursor: pointer; transition: transform 0.1s; }
.todo-card:hover { transform: translateY(-2px); }
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
.progress-bar-fill { height: 100%; background: var(--success); transition: width 0.2s; }
</style>
```

写入：`engineering/apps/web/todo-app/src/components/TodoCard.vue`

- [ ] **Step 9.2.3：创建 src/components/FilterBar.vue**

```vue
<template>
  <div class="filter-bar">
    <input v-model="filter.search" class="form-input" placeholder="搜索..." @input="emit('filter', filter)" />
    <select v-model="filter.status" class="form-select" @change="emit('filter', filter)">
      <option value="all">全部</option>
      <option value="open">未关闭</option>
      <option value="closed">已关闭</option>
      <option value="archived">已归档</option>
    </select>
    <select v-model="filter.priority" class="form-select" @change="emit('filter', filter)">
      <option value="-1">全部优先级</option>
      <option v-for="(l, i) in PRIORITY_LABELS" :key="i" :value="i">{{ l }}</option>
    </select>
    <select v-model="filter.group_id" class="form-select" @change="emit('filter', filter)">
      <option value="-1">全部分组</option>
      <option v-for="g in groups" :key="g.id" :value="g.id">{{ g.name }}</option>
    </select>
    <button class="btn btn-primary btn-sm" @click="emit('new')">+ 新建</button>
  </div>
</template>

<script setup>
import { reactive } from 'vue'

const props = defineProps({ groups: Array })
const emit = defineEmits(['filter', 'new'])

const PRIORITY_LABELS = ['紧急', '高', '中', '低', '无']
const filter = reactive({ search: '', status: 'all', priority: -1, group_id: -1, sort: 'sort_order', sort_desc: 0 })
</script>

<style scoped>
.filter-bar { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; padding: 12px 24px; background: var(--bg-elev); border-bottom: 1px solid var(--border); }
.form-input, .form-select { width: auto; }
</style>
```

写入：`engineering/apps/web/todo-app/src/components/FilterBar.vue`

- [ ] **Step 9.2.4：创建 src/components/CreateDialog.vue**

```vue
<template>
  <div v-if="show" class="modal-mask" @click.self="show = false">
    <div class="modal">
      <h3>新建待办</h3>
      <label class="label-text">标题 *</label>
      <input v-model.trim="form.title" class="form-input" maxlength="255" @keyup.enter="submit" />
      <label class="label-text">描述</label>
      <textarea v-model="form.description" class="form-textarea" rows="4" maxlength="4000"></textarea>
      <label class="label-text">优先级</label>
      <select v-model="form.priority" class="form-select">
        <option v-for="(l, i) in PRIORITY_LABELS" :key="i" :value="i">{{ l }}</option>
      </select>
      <label class="label-text">截止日期（时间戳）</label>
      <input v-model="form.due_date" type="number" class="form-input" placeholder="0=无" />
      <label class="label-text">分组</label>
      <select v-model="form.group_id" class="form-select">
        <option :value="0">未分组</option>
        <option v-for="g in groups" :key="g.id" :value="g.id">{{ g.name }}</option>
      </select>
      <label class="label-text">标签（逗号分隔）</label>
      <input v-model="form.labels" class="form-input" placeholder="bug, urgent" />
      <div style="display:flex; gap:8px; margin-top:16px;">
        <button class="btn btn-primary" :disabled="!form.title" @click="submit">创建</button>
        <button class="btn btn-secondary" @click="show = false">取消</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { reactive } from 'vue'

const props = defineProps({ groups: Array })
const emit = defineEmits(['created'])

const show = defineModel({ type: Boolean, default: false })
const PRIORITY_LABELS = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']

const form = reactive({ title: '', description: '', priority: 4, due_date: 0, group_id: 0, labels: '' })

function submit() {
  if (!form.title) return
  const labels = form.labels ? form.labels.split(',').map(s => s.trim()).filter(Boolean) : []
  emit('created', { ...form, labels })
  form.title = ''; form.description = ''; form.priority = 4
  form.due_date = 0; form.group_id = 0; form.labels = ''
  show.value = false
}
</script>
```

写入：`engineering/apps/web/todo-app/src/components/CreateDialog.vue`

- [ ] **Step 9.2.5：创建 src/components/DetailPanel.vue**

```vue
<template>
  <div v-if="todo" class="detail-panel">
    <div class="detail-header">
      <span :class="['priority-badge', 'priority-' + todo.priority]">{{ priorityLabel }}</span>
      <h2>{{ todo.title }}</h2>
      <span class="todo-id">#{{ todo.id }}</span>
    </div>
    <div class="detail-body">
      <p v-if="todo.description" class="description">{{ todo.description }}</p>
      <p v-else class="description empty-desc">暂无描述</p>
      <div class="todo-meta">
        <span v-if="todo.due_date && todo.due_date > 0" :class="['due-date', isOverdue ? 'overdue' : '']">
          📅 截止：{{ formatDate(todo.due_date) }}
        </span>
        <span v-for="label in labels" :key="label" class="label">{{ label }}</span>
      </div>

      <!-- Checklist -->
      <Checklist :todo-id="todo.id" :items="todo.checklist || []" @updated="$emit('updated')" />

      <!-- 评论 -->
      <Comments :todo-id="todo.id" :comments="todo.comments || []" @updated="$emit('updated')" />

      <!-- OPSX 变更按钮 -->
      <button class="btn btn-secondary btn-sm" style="margin-top:16px" @click="createChange">
        🔀 创建 OPSX 变更
      </button>
    </div>
  </div>
</template>

<script setup>
import { computed, inject } from 'vue'
import api from '../api.js'
import Checklist from './Checklist.vue'
import Comments from './Comments.vue'

const props = defineProps({ todo: Object })
const emit = defineEmits(['updated'])
const showToast = inject('showToast')

const PRIORITY_LABELS = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']
const priorityLabel = computed(() => PRIORITY_LABELS[props.todo?.priority] || '⚪无')
const labels = computed(() => {
  if (!props.todo?.labels) return []
  if (Array.isArray(props.todo.labels)) return props.todo.labels
  try { return JSON.parse(props.todo.labels) } catch { return [] }
})
const now = Date.now() / 1000
const isOverdue = computed(() => props.todo?.due_date > 0 && props.todo.due_date < now)

function formatDate(ts) { return new Date(ts * 1000).toLocaleDateString() }

async function createChange() {
  const r = await api.createChange(props.todo.id)
  if (r.code === 0) showToast(`变更已创建: ${r.data.change_id}`)
  else showToast(r.msg || '创建变更失败', 'error')
}
</script>

<style scoped>
.detail-panel { padding: 24px; }
.detail-header { margin-bottom: 16px; }
.detail-header h2 { margin: 8px 0; font-size: 20px; }
.todo-id { color: var(--text-muted); font-size: 12px; }
.description { background: var(--bg-elev); padding: 12px; border-radius: var(--radius); white-space: pre-wrap; margin-bottom: 12px; }
.empty-desc { color: var(--text-muted); font-style: italic; }
.todo-meta { display: flex; gap: 6px; align-items: center; margin-bottom: 16px; }
</style>
```

写入：`engineering/apps/web/todo-app/src/components/DetailPanel.vue`

- [ ] **Step 9.2.6：创建 src/components/Checklist.vue**

```vue
<template>
  <div class="checklist">
    <h4>待办项 {{ doneCount }}/{{ items.length }}</h4>
    <div v-for="item in items" :key="item.id" class="checklist-item">
      <input type="checkbox" :checked="item.done" @change="toggle(item.id)" />
      <span :class="item.done ? 'done' : ''">{{ item.text }}</span>
      <button class="btn-icon" @click="remove(item.id)">×</button>
    </div>
    <div class="checklist-add">
      <input v-model.trim="newText" class="form-input" placeholder="新增待办项..." @keyup.enter="add" />
      <button class="btn btn-secondary btn-sm" @click="add" :disabled="!newText">添加</button>
    </div>
  </div>
</template>

<script setup>
import { ref, inject } from 'vue'
import api from '../api.js'

const props = defineProps({ 'todo-id': Number, items: Array })
const emit = defineEmits(['updated'])
const showToast = inject('showToast')
const newText = ref('')
const doneCount = computed(() => props.items.filter(i => i.done).length)

import { computed } from 'vue'

async function add() {
  if (!newText.value) return
  const r = await api.addChecklist(props.todoId, newText.value)
  if (r.code === 0) { emit('updated'); newText.value = '' }
  else showToast(r.msg || '添加失败', 'error')
}
async function toggle(itemId) {
  const r = await api.toggleChecklist(props.todoId, itemId)
  if (r.code === 0) emit('updated')
}
async function remove(itemId) {
  if (!confirm('确认删除?')) return
  const r = await api.removeChecklist(props.todoId, itemId)
  if (r.code === 0) emit('updated')
}
</script>

<style scoped>
.checklist { margin: 16px 0; padding-top: 16px; border-top: 1px solid var(--border); }
.checklist h4 { margin-bottom: 8px; font-size: 14px; }
.checklist-item { display: flex; align-items: center; gap: 8px; padding: 4px 0; }
.checklist-item span { flex: 1; }
.checklist-item span.done { text-decoration: line-through; color: var(--text-muted); }
.checklist-add { display: flex; gap: 8px; margin-top: 8px; }
</style>
```

写入：`engineering/apps/web/todo-app/src/components/Checklist.vue`

- [ ] **Step 9.2.7：创建 src/components/Comments.vue**

```vue
<template>
  <div class="comments">
    <h4>评论 ({{ items.length }})</h4>
    <div v-for="c in items" :key="c.id" class="comment">
      <span class="comment-text">{{ c.text }}</span>
      <span class="comment-time">{{ formatTime(c.created_at) }}</span>
      <button class="btn-icon" @click="remove(c.id)">×</button>
    </div>
    <div class="comment-add">
      <input v-model.trim="newText" class="form-input" placeholder="添加评论..." @keyup.enter="add" />
      <button class="btn btn-secondary btn-sm" @click="add" :disabled="!newText">发送</button>
    </div>
  </div>
</template>

<script setup>
import { ref, inject } from 'vue'
import api from '../api.js'

const props = defineProps({ 'todo-id': Number, items: Array })
const emit = defineEmits(['updated'])
const showToast = inject('showToast')
const newText = ref('')

async function add() {
  if (!newText.value) return
  const r = await api.addComment(props.todoId, newText.value)
  if (r.code === 0) { emit('updated'); newText.value = '' }
  else showToast(r.msg || '发送失败', 'error')
}
async function remove(cid) {
  const r = await api.deleteComment(props.todoId, cid)
  if (r.code === 0) emit('updated')
}
function formatTime(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getMonth()+1}/${d.getDate()} ${d.getHours()}:${String(d.getMinutes()).padStart(2,'0')}`
}
</script>

<style scoped>
.comments { margin: 16px 0; padding-top: 16px; border-top: 1px solid var(--border); }
.comments h4 { margin-bottom: 8px; font-size: 14px; }
.comment { display: flex; align-items: center; gap: 8px; padding: 8px 0; border-bottom: 1px solid var(--border); }
.comment-text { flex: 1; }
.comment-time { font-size: 12px; color: var(--text-muted); }
.comment-add { display: flex; gap: 8px; margin-top: 8px; }
</style>
```

写入：`engineering/apps/web/todo-app/src/components/Comments.vue`

#### 9.3 视图组件

- [ ] **Step 9.3.1：创建 src/views/ListView.vue**

```vue
<template>
  <div class="list-view">
    <FilterBar :groups="groups" @filter="load" @new="showCreate = true" />
    <div class="list-body">
      <div class="todo-list">
        <TodoCard v-for="todo in todos" :key="todo.id" :todo="todo" @click="select(todo)" />
        <div v-if="todos.length === 0" class="empty">暂无待办</div>
      </div>
      <DetailPanel v-if="current" :todo="current" @updated="reloadCurrent" />
    </div>
    <CreateDialog v-model="showCreate" :groups="groups" @created="onCreated" />
  </div>
</template>

<script setup>
import { ref, onMounted, inject } from 'vue'
import api from '../api.js'
import FilterBar from '../components/FilterBar.vue'
import TodoCard from '../components/TodoCard.vue'
import DetailPanel from '../components/DetailPanel.vue'
import CreateDialog from '../components/CreateDialog.vue'

const showToast = inject('showToast')
const todos = ref([])
const current = ref(null)
const showCreate = ref(false)
const groups = ref([])
const filter = ref({ status: 'all', priority: -1, group_id: -1, search: '', sort: 'sort_order' })

async function load(f) {
  if (f) filter.value = { ...filter.value, ...f }
  const params = { ...filter.value, page: 1, per_page: 100 }
  const r = await api.list(params)
  if (r.code === 0) todos.value = r.data.items
  else showToast('加载失败', 'error')
}

async function select(todo) {
  const r = await api.get(todo.id)
  if (r.code === 0) current.value = r.data
}

async function reloadCurrent() {
  if (current.value) await select(current.value)
  await load()
}

async function onCreated(form) {
  const r = await api.create(form)
  if (r.code === 0) { showToast('已创建'); await load(); await select(r.data) }
  else showToast(r.msg || '创建失败', 'error')
}

onMounted(async () => {
  const rg = await api.listGroups()
  if (rg.code === 0) groups.value = rg.data
  await load()
})
</script>

<style scoped>
.list-view { height: 100vh; display: flex; flex-direction: column; }
.list-body { display: grid; grid-template-columns: 360px 1fr; flex: 1; overflow: hidden; }
.todo-list { overflow-y: auto; padding: 12px; display: flex; flex-direction: column; gap: 8px; }
.empty { text-align: center; color: var(--text-muted); padding: 40px; }
</style>
```

写入：`engineering/apps/web/todo-app/src/views/ListView.vue`

- [ ] **Step 9.3.2：创建 src/views/BoardView.vue**

```vue
<template>
  <div class="board-view">
    <div class="board-controls">
      <select v-model="groupBy" class="form-select">
        <option value="priority">按优先级</option>
        <option value="group">按分组</option>
      </select>
      <button class="btn btn-primary btn-sm" @click="showCreate = true">+ 新建</button>
    </div>
    <div class="board-columns">
      <div v-for="col in columns" :key="col.key" class="board-column">
        <div class="column-header">
          <span>{{ col.name }}</span>
          <span class="column-count">{{ col.todos.length }}</span>
        </div>
        <div class="column-todos">
          <TodoCard v-for="todo in col.todos" :key="todo.id" :todo="todo" @click="select(todo)" />
          <div v-if="col.todos.length === 0" class="column-empty">无待办</div>
        </div>
      </div>
    </div>
    <DetailPanel v-if="current" :todo="current" @updated="loadData" />
    <CreateDialog v-model="showCreate" :groups="groups" @created="onCreated" />
  </div>
</template>

<script setup>
import { ref, computed, onMounted, inject } from 'vue'
import api from '../api.js'
import TodoCard from '../components/TodoCard.vue'
import DetailPanel from '../components/DetailPanel.vue'
import CreateDialog from '../components/CreateDialog.vue'

const showToast = inject('showToast')
const todos = ref([])
const groups = ref([])
const current = ref(null)
const showCreate = ref(false)
const groupBy = ref('priority')

const PRIORITY_NAMES = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']

const columns = computed(() => {
  if (groupBy.value === 'priority') {
    return PRIORITY_NAMES.map((name, i) => ({
      key: i, name, todos: todos.value.filter(t => t.priority === i && t.status === 'open')
    }))
  } else {
    const cols = [{ key: 0, name: '未分组', todos: todos.value.filter(t => t.group_id === 0 && t.status === 'open') }]
    groups.value.forEach(g => {
      cols.push({ key: g.id, name: g.name, todos: todos.value.filter(t => t.group_id === g.id && t.status === 'open') })
    })
    return cols
  }
})

async function loadData() {
  const r = await api.list({ status: 'all', per_page: 1000 })
  if (r.code === 0) todos.value = r.data.items
  const rg = await api.listGroups()
  if (rg.code === 0) groups.value = rg.data
}

async function select(todo) {
  const r = await api.get(todo.id)
  if (r.code === 0) current.value = r.data
}

async function onCreated(form) {
  const r = await api.create(form)
  if (r.code === 0) { showToast('已创建'); await loadData() }
  else showToast(r.msg || '创建失败', 'error')
}

onMounted(loadData)
</script>

<style scoped>
.board-view { height: 100vh; display: flex; flex-direction: column; }
.board-controls { display: flex; gap: 8px; padding: 12px 24px; background: var(--bg-elev); border-bottom: 1px solid var(--border); }
.board-columns { display: flex; gap: 16px; padding: 16px; overflow-x: auto; flex: 1; }
.board-column { min-width: 280px; flex: 1; max-width: 320px; background: var(--bg-elev); border-radius: var(--radius); padding: 12px; }
.column-header { display: flex; justify-content: space-between; font-weight: 600; margin-bottom: 12px; }
.column-count { color: var(--text-muted); font-weight: normal; font-size: 12px; }
.column-todos { display: flex; flex-direction: column; gap: 8px; }
.column-empty { text-align: center; color: var(--text-muted); font-size: 12px; padding: 20px 0; }
</style>
```

写入：`engineering/apps/web/todo-app/src/views/BoardView.vue`

- [ ] **Step 9.3.3：创建 src/views/StatsView.vue**

```vue
<template>
  <div class="stats-view">
    <h2>📈 统计看板</h2>
    <div v-if="stats" class="stats-grid">
      <div class="stat-card card">
        <div class="stat-value">{{ stats.total }}</div>
        <div class="stat-label">总数</div>
      </div>
      <div class="stat-card card">
        <div class="stat-value" style="color:var(--success)">{{ stats.open }}</div>
        <div class="stat-label">未关闭</div>
      </div>
      <div class="stat-card card">
        <div class="stat-value" style="color:var(--closed)">{{ stats.closed }}</div>
        <div class="stat-label">已关闭</div>
      </div>
      <div class="stat-card card">
        <div class="stat-value" style="color:var(--danger)">{{ stats.overdue }}</div>
        <div class="stat-label">已过期</div>
      </div>
      <div class="stat-card card">
        <div class="stat-value" style="color:var(--warning)">{{ stats.due_today }}</div>
        <div class="stat-label">今日到期</div>
      </div>
      <div class="stat-card card">
        <div class="stat-value">{{ (stats.completion_rate * 100).toFixed(1) }}%</div>
        <div class="stat-label">完成率</div>
      </div>
    </div>
    <div class="stats-detail">
      <div class="card" style="flex:1">
        <h3>优先级分布</h3>
        <div v-for="p in stats?.by_priority || []" :key="p.priority" class="bar-row">
          <span class="bar-label">{{ PRIORITY_NAMES[p.priority] }}</span>
          <div class="bar">
            <div class="bar-fill" :style="{ width: (p.count / maxPriority * 100) + '%' }"></div>
          </div>
          <span class="bar-count">{{ p.count }}</span>
        </div>
      </div>
      <div class="card" style="flex:1">
        <h3>分组分布</h3>
        <div v-if="stats?.by_group?.length" v-for="g in stats.by_group" :key="g.group_id" class="bar-row">
          <span class="bar-label">{{ g.group_name }}</span>
          <div class="bar">
            <div class="bar-fill" :style="{ width: (g.count / maxGroup * 100) + '%', background: '#2f81f7' }"></div>
          </div>
          <span class="bar-count">{{ g.count }}</span>
        </div>
        <div v-else class="empty">暂无分组数据</div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import api from '../api.js'

const stats = ref(null)
const PRIORITY_NAMES = ['紧急', '高', '中', '低', '无']

const maxPriority = computed(() => Math.max(...(stats.value?.by_priority?.map(p => p.count) || [1])))
const maxGroup = computed(() => Math.max(...(stats.value?.by_group?.map(g => g.count) || [1])))

onMounted(async () => {
  const r = await api.stats()
  if (r.code === 0) stats.value = r.data
})
</script>

<style scoped>
.stats-view { padding: 24px; max-width: 900px; margin: 0 auto; }
.stats-view h2 { margin-bottom: 24px; }
.stats-view h3 { margin: 16px 0 12px; font-size: 14px; }
.stats-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 16px; margin-bottom: 24px; }
.stat-card { text-align: center; padding: 24px; }
.stat-value { font-size: 36px; font-weight: 700; }
.stat-label { color: var(--text-muted); font-size: 13px; margin-top: 4px; }
.stats-detail { display: flex; gap: 16px; }
.bar-row { display: flex; align-items: center; gap: 8px; margin: 6px 0; }
.bar-label { width: 60px; font-size: 13px; }
.bar { flex: 1; height: 8px; background: var(--bg-elev2); border-radius: 4px; overflow: hidden; }
.bar-fill { height: 100%; background: var(--success); transition: width 0.3s; }
.bar-count { width: 30px; text-align: right; font-size: 12px; color: var(--text-muted); }
.empty { color: var(--text-muted); font-size: 13px; }
</style>
```

写入：`engineering/apps/web/todo-app/src/views/StatsView.vue`

- [ ] **Step 9.3.4：创建 src/views/GroupManager.vue**

```vue
<template>
  <div class="group-manager">
    <div class="header">
      <h2>🏷️ 分组管理</h2>
      <button class="btn btn-primary btn-sm" @click="showCreate = true">+ 新建分组</button>
    </div>
    <div class="group-list">
      <div v-for="g in groups" :key="g.id" class="group-item card">
        <span class="group-color" :style="{ background: g.color }"></span>
        <span class="group-name">{{ g.name }}</span>
        <div class="group-actions">
          <button class="btn btn-secondary btn-sm" @click="startEdit(g)">编辑</button>
          <button class="btn btn-danger btn-sm" @click="remove(g.id)">删除</button>
        </div>
      </div>
      <div v-if="groups.length === 0" class="empty">暂无分组</div>
    </div>

    <!-- 新建/编辑弹窗 -->
    <div v-if="showCreate || editing" class="modal-mask" @click.self="closeDialog">
      <div class="modal">
        <h3>{{ editing ? '编辑分组' : '新建分组' }}</h3>
        <label class="label-text">名称</label>
        <input v-model="form.name" class="form-input" maxlength="64" />
        <label class="label-text">颜色</label>
        <div class="color-picker">
          <button v-for="c in COLORS" :key="c" :class="['color-btn', form.color === c ? 'selected' : '']"
            :style="{ background: c }" @click="form.color = c"></button>
        </div>
        <div style="display:flex; gap:8px; margin-top:16px;">
          <button class="btn btn-primary" :disabled="!form.name" @click="submit">{{ editing ? '保存' : '创建' }}</button>
          <button class="btn btn-secondary" @click="closeDialog">取消</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, inject } from 'vue'
import api from '../api.js'

const showToast = inject('showToast')
const groups = ref([])
const showCreate = ref(false)
const editing = ref(null)
const form = reactive({ name: '', color: '#4A90D9' })

const COLORS = ['#f85149','#d29922','#3fb950','#2f81f7','#a371f7','#8957e5','#ff7b72','#79c0ff']

async function load() {
  const r = await api.listGroups()
  if (r.code === 0) groups.value = r.data
}
function startEdit(g) { editing.value = g; form.name = g.name; form.color = g.color }
function closeDialog() { showCreate.value = false; editing.value = null; form.name = ''; form.color = '#4A90D9' }

async function submit() {
  if (!form.name) return
  if (editing.value) {
    const r = await api.updateGroup(editing.value.id, form)
    if (r.code === 0) { showToast('已保存'); closeDialog(); await load() }
    else showToast(r.msg || '保存失败', 'error')
  } else {
    const r = await api.createGroup(form)
    if (r.code === 0) { showToast('已创建'); closeDialog(); await load() }
    else showToast(r.msg || '创建失败', 'error')
  }
}
async function remove(id) {
  if (!confirm('确认删除分组？')) return
  const r = await api.deleteGroup(id)
  if (r.code === 0) { showToast('已删除'); await load() }
  else showToast(r.msg || '删除失败', 'error')
}

onMounted(load)
</script>

<style scoped>
.group-manager { padding: 24px; max-width: 600px; margin: 0 auto; }
.header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; }
.group-list { display: flex; flex-direction: column; gap: 8px; }
.group-item { display: flex; align-items: center; gap: 12px; }
.group-color { width: 20px; height: 20px; border-radius: 4px; flex-shrink: 0; }
.group-name { flex: 1; font-weight: 500; }
.group-actions { display: flex; gap: 8px; }
.empty { text-align: center; color: var(--text-muted); padding: 40px; }
.color-picker { display: flex; gap: 8px; flex-wrap: wrap; }
.color-btn { width: 28px; height: 28px; border-radius: 4px; border: 2px solid transparent; cursor: pointer; }
.color-btn.selected { border-color: #fff; }
</style>
```

写入：`engineering/apps/web/todo-app/src/views/GroupManager.vue`

#### 9.4 验证

- [ ] **Step 9.4.1：安装依赖并构建**

```bash
cd engineering/apps/web/todo-app
npm install
npm run build
```
预期：`dist/` 目录生成产物

- [ ] **Step 9.4.2：提交前端代码**

```bash
git add engineering/apps/web/todo-app/
git commit -m "feat(todo-app): 前端工程化完成（Vue 3 + Vite + 5 个视图 + 7 个组件）"
```

---

### Task 10：端到端集成联调

**文件：**
- 修改：`engineering/apps/todo-app/CMakeLists.txt`（添加前端构建）

**接口：**
- 消费：Task 9 的前端项目
- 产生：完整 todo-app 系统

- [ ] **Step 10.1：更新 CMakeLists.txt 添加前端构建目标**

编辑 `engineering/apps/todo-app/CMakeLists.txt`，在文件末尾追加：

```cmake
# 前端构建（可选，需 Node.js）
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_custom_target(todo-app-frontend
        COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../web/todo-app/node_modules/.bin/vite build
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../web/todo-app
        COMMENT "Building Vue 3 frontend..."
        DEPENDS project_includes
    )
    add_dependencies(todo-app todo-app-frontend)
endif()
```

- [ ] **Step 10.2：完整编译**

```bash
cmake --build build/engineering --target todo-app
```
预期：编译成功

- [ ] **Step 10.3：启动服务器测试**

```bash
# 在 build/engineering/apps/todo-app 目录下运行
./todo-app.exe -p 8080 -d todo_app.json
```

```bash
# 测试 API（另开终端）
curl http://localhost:8080/api/todos
curl -X POST http://localhost:8080/api/todos -H "Content-Type: application/json" -d "{\"title\":\"测试待办\",\"priority\":0,\"due_date\":$(date +%s)}"
curl http://localhost:8080/api/todos/stats
curl http://localhost:8080/api/groups
curl -X POST http://localhost:8080/api/groups -H "Content-Type: application/json" -d "{\"name\":\"工作\",\"color\":\"#f85149\"}"
```

预期：
- `GET /api/todos` 返回 `{"code":0,"data":{"items":[],"total":0,...}}`
- `POST /api/todos` 创建成功
- `GET /api/todos/stats` 返回统计 JSON
- `POST /api/groups` 创建分组成功

- [ ] **Step 10.4：测试旧数据迁移**

```bash
# 创建旧格式文件
echo '{"next_issue_id":3,"issues":[{"id":1,"title":"旧待办","status":"open"}],"checklist":[]}' > issue_tool.json
./todo-app.exe
```
预期：控制台输出 `[迁移]` 日志，`todo_app.json` 生成，`issue_tool.json.bak` 备份

- [ ] **Step 10.5：提交**

```bash
git add engineering/apps/todo-app/CMakeLists.txt
git commit -m "feat(todo-app): 集成联调完成，端到端验证通过"
```

---

## 自审清单

**1. Spec 覆盖：**
- [x] 优先级（P0-P4）：`todo_model.c` + `todo_handler.c`（`priority` 字段）
- [x] 截止日期（`due_date` 筛选 + 过期高亮）：`todo_model.c`（`due_before`/`due_after`）
- [x] 分组管理（CRUD）：`group_t` + `group_*` 函数 + `/api/groups` 路由
- [x] 评论（CRUD）：`comment_t` + `comment_*` 函数 + `/api/todos/:id/comments`
- [x] 拖拽排序（`sort_order`）：`todo_update_sort` + `/api/todos/:id/sort`
- [x] 统计看板：`todo_get_stats` + `/api/todos/stats`
- [x] OPSX 变更桥接：`todo_create_change` + `/api/todos/:id/create-change`
- [x] 看板视图：Vue `BoardView.vue`（`group_by=priority/group`）
- [x] 旧数据迁移：`todo_migration.c`

**2. 类型一致性：**
- [x] `todo_t` 结构体字段：`id`, `title`, `description`, `status`, `labels`, `priority`, `due_date`, `group_id`, `sort_order`, `created_at`, `updated_at`
- [x] `group_t`：`id`, `name`, `color`, `sort_order`, `created_at`
- [x] `comment_t`：`id`, `todo_id`, `text`, `created_at`
- [x] `checklist_item_t`：`todo_id`（从 `issue_id` 重命名）
- [x] API 路由：`/api/todos/*`（非 `/api/issues`）

**3. 占位符检查：**
- [x] 所有函数有完整实现
- [x] 所有测试有实际断言
- [x] 所有步骤有实际命令
- [x] 无 "TBD"、"TODO"、"fill in details"
