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
    <DetailPanel v-if="current" :todo="current" @updated="loadData" @close="current = null" />
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
.board-controls { display: flex; gap: 8px; padding: 12px 24px; background: var(--bg-elev); border-bottom: 1px solid var(--border); align-items: center; }
.board-columns { display: flex; gap: 16px; padding: 16px; overflow-x: auto; flex: 1; }
.board-column { min-width: 280px; flex: 1; max-width: 320px; background: var(--bg-elev); border-radius: var(--radius); padding: 12px; border: 1px solid var(--border); }
.column-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; padding-bottom: 8px; border-bottom: 2px solid var(--border); }
.column-count { color: var(--text-muted); font-weight: normal; font-size: 12px; background: var(--bg-elev2); padding: 2px 8px; border-radius: 10px; }
.column-todos { display: flex; flex-direction: column; gap: 8px; }
.column-empty { text-align: center; color: var(--text-muted); font-size: 12px; padding: 20px 0; border: 2px dashed var(--border); border-radius: var(--radius); }
</style>
