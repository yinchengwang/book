<template>
  <div class="board-view">
    <!-- 视图切换器 -->
    <ViewSelector @view-changed="onViewChanged" />

    <!-- 看板内容 -->
    <div class="board-controls">
      <span class="board-hint">拖拽移动任务</span>
      <button class="btn btn-primary btn-sm" @click="showCreate = true">+ 新建</button>
    </div>

    <div class="board-columns">
      <div v-for="col in columns" :key="col.key" class="board-column" @dragover.prevent @drop="onDrop($event, col.key)">
        <div class="column-header">
          <span>{{ col.name }}</span>
          <span class="column-count">{{ col.todos.length }}</span>
        </div>
        <div class="column-todos">
          <div
            v-for="todo in col.todos"
            :key="todo.id"
            class="board-card"
            draggable="true"
            @dragstart="onDragStart($event, todo)"
            @click="select(todo)"
          >
            <div class="card-title">{{ todo.title }}</div>
            <div v-if="todo.due_date" class="card-due">{{ formatDate(todo.due_date) }}</div>
            <div v-if="todo.priority !== 2" class="card-priority">{{ priorityIcon(todo.priority) }}</div>
          </div>
          <div v-if="col.todos.length === 0" class="column-empty">无待办</div>
        </div>
      </div>
    </div>

    <DetailPanel v-if="current" :todo="current" @updated="loadData" @close="current = null" />
    <CreateDialog v-model="showCreate" :groups="groups" @created="onCreated" />
  </div>
</template>

<script setup>
import { ref, computed, onMounted, inject } from 'vue'
import api from '../api.js'
import ViewSelector from '../components/ViewSelector.vue'

const showToast = inject('showToast')
const todos = ref([])
const groups = ref([])
const fields = ref([])
const current = ref(null)
const showCreate = ref(false)
const currentView = ref(null)
const draggedTodo = ref(null)

const PRIORITY_NAMES = ['🔴紧急', '🟠高', '🟡中', '🟢低', '⚪无']

// 从视图配置获取分组字段
const groupFieldId = computed(() => {
  const cfg = currentView.value?.config || {}
  return parseInt(cfg.group_field) || 3  // 默认按状态分组
})

const columns = computed(() => {
  const fid = groupFieldId.value

  if (fid === 3) {
    // 按状态分组
    return [
      { key: 'open', name: '📋 待办', todos: todos.value.filter(t => t.status === 'open') },
      { key: 'closed', name: '✅ 已完成', todos: todos.value.filter(t => t.status === 'closed') },
      { key: 'archived', name: '📦 已归档', todos: todos.value.filter(t => t.status === 'archived') },
    ]
  } else if (fid === 4) {
    // 按优先级分组
    return PRIORITY_NAMES.map((name, i) => ({
      key: i, name, todos: todos.value.filter(t => t.priority === i && t.status === 'open')
    }))
  } else if (fid === 7) {
    // 按分组
    const cols = [{ key: 0, name: '未分组', todos: todos.value.filter(t => t.group_id === 0 && t.status === 'open') }]
    groups.value.forEach(g => {
      cols.push({ key: g.id, name: g.name, todos: todos.value.filter(t => t.group_id === g.id && t.status === 'open') })
    })
    return cols
  } else if (fid >= 10) {
    // 按自定义字段分组
    const fieldMap = new Map()
    fields.value.forEach(f => fieldMap.set(f.id, f))

    for (const todo of todos.value) {
      const val = todo.fields?.[fid] || '_none_'
      if (!columnsMap.has(val)) {
        columnsMap.set(val, { key: val, name: val === '_none_' ? '未填写' : String(val), todos: [] })
      }
      columnsMap.get(val).todos.push(todo)
    }

    return Array.from(columnsMap.values())
  }

  return [{ key: 'all', name: '全部', todos: todos.value.filter(t => t.status === 'open') }]
})

// 修复：columnsMap 应该在 computed 内部
const columnsFixed = computed(() => {
  const fid = groupFieldId.value
  let result = []

  if (fid === 3) {
    result = [
      { key: 'open', name: '📋 待办', todos: [] },
      { key: 'closed', name: '✅ 已完成', todos: [] },
      { key: 'archived', name: '📦 已归档', todos: [] },
    ]
  } else if (fid === 4) {
    result = PRIORITY_NAMES.map((name, i) => ({ key: i, name, todos: [] }))
  } else if (fid === 7) {
    result = [{ key: 0, name: '未分组', todos: [] }]
    groups.value.forEach(g => result.push({ key: g.id, name: g.name, todos: [] }))
  } else {
    result = [{ key: 'all', name: '全部', todos: [] }]
  }

  for (const todo of todos.value) {
    let key
    if (fid === 3) key = todo.status
    else if (fid === 4) key = todo.priority
    else if (fid === 7) key = todo.group_id
    else if (fid >= 10) key = todo.fields?.[fid] || '_none_'
    else key = 'all'

    const col = result.find(c => String(c.key) === String(key))
    if (col) col.todos.push(todo)
    else result[0].todos.push(todo)
  }

  return result
})

const columns = computed(() => columnsFixed.value)

async function loadData() {
  const r = await api.list({ status: 'all', per_page: 1000 })
  if (r.code === 0) todos.value = r.data.items
  const rg = await api.listGroups()
  if (rg.code === 0) groups.value = rg.data
  const rf = await api.listFields()
  if (rf.code === 0) fields.value = rf.data
}

function onViewChanged(view) {
  currentView.value = view
  loadData()
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

// 拖拽
function onDragStart(e, todo) {
  draggedTodo.value = todo
  e.dataTransfer.effectAllowed = 'move'
}

async function onDrop(e, targetKey) {
  if (!draggedTodo.value) return
  const todo = draggedTodo.value
  draggedTodo.value = null

  // 根据分组字段更新 todo
  const fid = groupFieldId.value
  let updates = {}

  if (fid === 3) {
    updates.status = targetKey
  } else if (fid === 4) {
    updates.priority = parseInt(targetKey)
  } else if (fid === 7) {
    updates.group_id = parseInt(targetKey)
  }

  if (Object.keys(updates).length > 0) {
    const r = await api.update(todo.id, updates)
    if (r.code === 0) {
      showToast('已移动')
      loadData()
    }
  }
}

function priorityIcon(p) {
  const m = { 0: '🔴', 1: '🟠', 2: '🟡', 3: '🟢', 4: '⚪' }
  return m[p] || ''
}

function formatDate(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return d.toLocaleDateString('zh-CN', { month: 'short', day: 'numeric' })
}

onMounted(loadData)
</script>

<style scoped>
.board-view { height: 100vh; display: flex; flex-direction: column; }
.board-controls {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 24px;
  background: var(--card-bg, #fff);
  border-bottom: 1px solid var(--border-color, #e0e0e0);
}
.board-hint { font-size: 12px; color: #888; }
.board-columns { display: flex; gap: 16px; padding: 16px; overflow-x: auto; flex: 1; }
.board-column {
  min-width: 280px; flex: 1; max-width: 320px;
  background: var(--bg-elev, #f5f7fa);
  border-radius: 8px; padding: 12px;
  border: 1px solid var(--border-color, #e0e0e0);
}
.column-header {
  display: flex; justify-content: space-between; align-items: center;
  margin-bottom: 12px; padding-bottom: 8px;
  border-bottom: 2px solid var(--primary-color, #4A90D9);
}
.column-count {
  color: var(--text-muted, #888); font-weight: normal; font-size: 12px;
  background: var(--bg-elev2, #eee); padding: 2px 8px; border-radius: 10px;
}
.column-todos { display: flex; flex-direction: column; gap: 8px; min-height: 100px; }
.column-empty {
  text-align: center; color: var(--text-muted, #888); font-size: 12px;
  padding: 20px 0; border: 2px dashed var(--border-color, #e0e0e0); border-radius: 8px;
}
.board-card {
  background: var(--card-bg, #fff); padding: 12px; border-radius: 6px;
  border: 1px solid var(--border-color, #e0e0e0); cursor: pointer;
  transition: box-shadow 0.2s;
}
.board-card:hover { box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
.card-title { font-size: 14px; font-weight: 500; margin-bottom: 4px; }
.card-due { font-size: 11px; color: #888; }
.card-priority { position: absolute; top: 8px; right: 8px; }
.board-card { position: relative; }
</style>
