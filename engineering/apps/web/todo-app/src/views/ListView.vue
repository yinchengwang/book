<template>
  <div class="list-view">
    <FilterBar :groups="groups" @filter="load" @new="showCreate = true" />
    <div class="list-body">
      <SkeletonLoader v-if="loading" :count="5" />
      <div v-else class="todo-list">
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
      <DetailPanel v-if="current" :todo="current" @updated="reloadCurrent" @close="current = null" />
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
import SkeletonLoader from '../components/SkeletonLoader.vue'
import EmptyState from '../components/EmptyState.vue'

const showToast = inject('showToast')
const todos = ref([])
const current = ref(null)
const showCreate = ref(false)
const groups = ref([])
const loading = ref(true)

async function load(f) {
  if (f) filter.value = { ...filter.value, ...f }
  loading.value = true
  const params = { ...filter.value, page: 1, per_page: 100 }
  const r = await api.list(params)
  if (r.code === 0) todos.value = r.data.items
  else showToast('加载失败', 'error')
  loading.value = false
}

const filter = ref({ status: 'all', priority: -1, group_id: -1, search: '' })

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
.todo-list { overflow-y: auto; padding: 12px; }
.todo-list-inner { display: flex; flex-direction: column; gap: 8px; }
</style>
