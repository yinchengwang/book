<template>
  <div class="table-view">
    <!-- 视图切换器 -->
    <ViewSelector @view-changed="onViewChanged" />

    <!-- 视图内容 -->
    <div class="view-content">
      <!-- 表格视图 -->
      <template v-if="currentView?.type === 'table'">
        <!-- 分组模式 -->
        <div v-if="groupConfig" class="grouped-view">
          <div v-for="group in groupedTodos" :key="group.key" class="todo-group">
            <div class="group-header" @click="group.collapsed = !group.collapsed">
              <span class="group-toggle">{{ group.collapsed ? '▶' : '▼' }}</span>
              <span class="group-label">{{ group.label }}</span>
              <span class="group-count">({{ group.items.length }})</span>
            </div>
            <div v-show="!group.collapsed" class="data-table">
              <table>
                <thead>
                  <tr>
                    <th v-for="col in visibleColumns" :key="col.id" :style="{ width: col.width + 'px' }">
                      {{ col.name }}
                    </th>
                  </tr>
                </thead>
                <tbody>
                  <tr v-for="todo in group.items" :key="todo.id" @click="select(todo)">
                    <td v-for="col in visibleColumns" :key="col.id">
                      <template v-if="col.id === 1">{{ todo.title }}</template>
                      <template v-else-if="col.id === 3">
                        <span :class="['status-badge', 'status-' + todo.status]">{{ statusLabel(todo.status) }}</span>
                      </template>
                      <template v-else-if="col.id === 4">{{ priorityLabel(todo.priority) }}</template>
                      <template v-else-if="col.id === 5">{{ formatDate(todo.due_date) }}</template>
                      <template v-else-if="col.id === 6">{{ todo.labels }}</template>
                      <template v-else-if="col.id === 8">{{ formatDate(todo.created_at) }}</template>
                      <template v-else-if="col.id === 9">{{ formatDate(todo.updated_at) }}</template>
                      <template v-else-if="col.id >= 10">{{ todo.fields?.[col.id] || '-' }}</template>
                      <template v-else>-</template>
                    </td>
                  </tr>
                </tbody>
              </table>
            </div>
          </div>
        </div>
        <!-- 非分组模式 -->
        <div v-else class="data-table">
          <table>
            <thead>
              <tr>
                <th v-for="col in visibleColumns" :key="col.id" :style="{ width: col.width + 'px' }">
                  {{ col.name }}
                </th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="todo in todos" :key="todo.id" @click="select(todo)">
                <td v-for="col in visibleColumns" :key="col.id">
                  <template v-if="col.id === 1">{{ todo.title }}</template>
                  <template v-else-if="col.id === 3">
                    <span :class="['status-badge', 'status-' + todo.status]">{{ statusLabel(todo.status) }}</span>
                  </template>
                  <template v-else-if="col.id === 4">{{ priorityLabel(todo.priority) }}</template>
                  <template v-else-if="col.id === 5">{{ formatDate(todo.due_date) }}</template>
                  <template v-else-if="col.id === 6">{{ todo.labels }}</template>
                  <template v-else-if="col.id === 8">{{ formatDate(todo.created_at) }}</template>
                  <template v-else-if="col.id === 9">{{ formatDate(todo.updated_at) }}</template>
                  <template v-else-if="col.id >= 10">{{ todo.fields?.[col.id] || '-' }}</template>
                  <template v-else>-</template>
                </td>
              </tr>
              <tr v-if="todos.length === 0">
                <td :colspan="visibleColumns.length" class="empty-row">暂无数据</td>
              </tr>
            </tbody>
          </table>
        </div>
      </template>

      <!-- 卡片列表视图（默认） -->
      <template v-else>
        <div class="card-list">
          <div class="list-body">
            <SkeletonLoader v-if="loading" :count="5" />
            <div v-else>
              <TransitionGroup v-if="todos.length > 0" name="todo-list" tag="div" class="todo-list-inner">
                <TodoCard v-for="todo in todos" :key="todo.id" :todo="todo" @click="select(todo)" />
              </TransitionGroup>
              <EmptyState
                v-else
                icon="📋"
                title="还没有待办"
                description="点击右上角按钮创建你的第一个待办事项"
                actionText="创建待办"
                @action="showCreate = true"
              />
            </div>
          </div>
          <DetailPanel v-if="current" :todo="current" @updated="reloadCurrent" @close="current = null" />
        </div>
      </template>
    </div>

    <!-- 创建按钮 -->
    <button class="fab" @click="showCreate = true" title="新建待办">+</button>

    <CreateDialog v-model="showCreate" :groups="groups" @created="onCreated" />
  </div>
</template>

<script setup>
import { ref, computed, onMounted, watch, inject } from 'vue'
import api from '../api.js'
import ViewSelector from '../components/ViewSelector.vue'
import TodoCard from '../components/TodoCard.vue'
import DetailPanel from '../components/DetailPanel.vue'
import CreateDialog from '../components/CreateDialog.vue'
import SkeletonLoader from '../components/SkeletonLoader.vue'
import EmptyState from '../components/EmptyState.vue'

const showToast = inject('showToast')
const todos = ref([])
const current = ref(null)
const showCreate = ref(false)
const groups = ref([])
const groupsMap = computed(() => {
  const m = {}
  for (const g of groups.value) m[g.id] = g
  return m
})
const loading = ref(true)
const fields = ref([])
const currentView = ref(null)

const filter = ref({ status: 'all', priority: -1, group_id: -1, search: '' })

// 分组配置
const groupConfig = computed(() => {
  const cfg = currentView.value?.config || {}
  if (cfg.group_by) return { fieldId: parseInt(cfg.group_by) }
  return null
})

// 按分组聚合数据
const groupedTodos = computed(() => {
  if (!groupConfig.value || !currentView.value) return []

  const fieldId = groupConfig.value.fieldId
  const groups = new Map()

  for (const todo of todos.value) {
    let key, label
    if (fieldId === 3) {
      key = todo.status
      label = statusLabel(todo.status)
    } else if (fieldId === 4) {
      key = String(todo.priority)
      label = priorityLabel(todo.priority)
    } else if (fieldId === 7) {
      key = String(todo.group_id)
      label = todo.group_id ? groupsMap.value[todo.group_id]?.name || `分组 ${todo.group_id}` : '未分组'
    } else if (fieldId >= 10) {
      key = todo.fields?.[fieldId] || '_none_'
      label = key === '_none_' ? '未填写' : String(key)
    } else {
      key = '_other_'
      label = '其他'
    }

    if (!groups.has(key)) {
      groups.set(key, { key, label, items: [], collapsed: false })
    }
    groups.get(key).items.push(todo)
  }

  return Array.from(groups.values()).sort((a, b) => a.key.localeCompare(b.key))
})

// 可见列
const visibleColumns = computed(() => {
  const cfg = currentView.value?.config || {}
  const visible = cfg.visible_fields || [1, 3, 4, 5]

  const colDefs = [
    { id: 1, name: '标题', width: 200 },
    { id: 3, name: '状态', width: 80 },
    { id: 4, name: '优先级', width: 70 },
    { id: 5, name: '截止日期', width: 100 },
    { id: 6, name: '标签', width: 120 },
    { id: 7, name: '分组', width: 80 },
    { id: 8, name: '创建时间', width: 120 },
    { id: 9, name: '更新时间', width: 120 },
  ]

  // 添加自定义字段
  for (const f of fields.value) {
    if (f.id >= 10) {
      colDefs.push({ id: f.id, name: f.name, width: 100 })
    }
  }

  return colDefs.filter(c => visible.includes(c.id))
})

async function loadFields() {
  const res = await api.listFields()
  if (res.code === 0) fields.value = res.data
}

async function load(f) {
  if (f) filter.value = { ...filter.value, ...f }
  loading.value = true
  const params = { ...filter.value, page: 1, per_page: 100 }
  const r = await api.list(params)
  if (r.code === 0) todos.value = r.data.items
  else showToast('加载失败', 'error')
  loading.value = false
}

async function select(todo) {
  const r = await api.get(todo.id)
  if (r.code === 0) current.value = r.data
}

async function reloadCurrent() {
  if (current.value) await select(current.value)
  await load()
}

async function onCreated(payload) {
  const { body, fields } = payload
  const r = await api.create(body)
  if (r.code === 0) {
    const todo = r.data
    // 如果有扩展字段，创建后设置
    if (fields && Object.keys(fields).length > 0) {
      await api.updateTodoFields(todo.id, fields)
    }
    showToast('已创建')
    await load()
    await select(todo.id)
  } else showToast(r.msg || '创建失败', 'error')
}

function onViewChanged(view) {
  currentView.value = view
  load()  // 重新加载以应用视图配置
}

function statusLabel(status) {
  const m = { open: '待办', closed: '完成', archived: '归档' }
  return m[status] || status
}

function priorityLabel(p) {
  const m = { 0: '🔴紧急', 1: '🟠高', 2: '🟡中', 3: '🟢低', 4: '⚪无' }
  return m[p] ?? '-'
}

function formatDate(ts) {
  if (!ts) return '-'
  const d = new Date(ts * 1000)
  return d.toLocaleDateString('zh-CN')
}

onMounted(async () => {
  const rg = await api.listGroups()
  if (rg.code === 0) groups.value = rg.data
  await loadFields()
  await load()
})
</script>

<style scoped>
.table-view {
  height: 100vh;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}
.view-content {
  flex: 1;
  overflow: hidden;
}
.card-list {
  height: 100%;
  display: grid;
  grid-template-columns: 1fr 400px;
}
.list-body {
  overflow-y: auto;
  padding: 12px;
}
.todo-list-inner {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

/* 表格视图样式 */
.data-table {
  height: 100%;
  overflow: auto;
  padding: 16px;
}
.data-table table {
  width: 100%;
  border-collapse: collapse;
  font-size: 14px;
}
.data-table th,
.data-table td {
  padding: 10px 12px;
  text-align: left;
  border-bottom: 1px solid var(--border-color, #e0e0e0);
}
.data-table th {
  background: var(--header-bg, #f8f9fa);
  font-weight: 600;
  position: sticky;
  top: 0;
  z-index: 1;
}
.data-table tr:hover td {
  background: var(--hover-bg, #f5f5f5);
  cursor: pointer;
}
.status-badge {
  display: inline-block;
  padding: 2px 8px;
  border-radius: 4px;
  font-size: 12px;
}
.status-open { background: #e3f2fd; color: #1976d2; }
.status-closed { background: #e8f5e9; color: #388e3c; }
.status-archived { background: #f5f5f5; color: #757575; }
.empty-row {
  text-align: center;
  color: #888;
  padding: 40px !important;
}

/* 浮动按钮 */
.fab {
  position: fixed;
  bottom: 24px;
  right: 24px;
  width: 56px;
  height: 56px;
  border-radius: 50%;
  background: var(--primary-color, #4A90D9);
  color: white;
  border: none;
  font-size: 28px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.2);
  cursor: pointer;
  transition: transform 0.2s;
  z-index: 100;
}
.fab:hover { transform: scale(1.1); }

/* 分组样式 */
.grouped-view {
  padding: 16px;
}
.todo-group {
  margin-bottom: 16px;
  border: 1px solid var(--border-color, #e0e0e0);
  border-radius: 8px;
  overflow: hidden;
}
.group-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 12px 16px;
  background: var(--header-bg, #f0f4f8);
  cursor: pointer;
  font-weight: 600;
}
.group-header:hover {
  background: var(--hover-bg, #e8eef4);
}
.group-toggle {
  font-size: 12px;
  color: #666;
}
.group-label {
  flex: 1;
}
.group-count {
  font-size: 12px;
  color: #888;
  font-weight: normal;
}
.group-header + .data-table {
  margin: 0;
}
</style>
