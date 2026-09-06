<template>
  <div class="list-view">
    <FilterBar :groups="groups" @filter="onFilter" @new="showCreate = true" />
    <div class="list-body">
      <SkeletonLoader v-if="loading" :count="5" />
      <div v-else ref="listContainer" class="todo-list" data-test="todo-list-container">
        <VueDraggable
          v-if="todos.length > 0"
          v-model="todosProxy"
          :animation="180"
          handle=".drag-handle"
          class="todo-list-inner"
          @end="onDragEnd"
        >
          <TodoCard v-for="todo in todos" :key="todo.id" :todo="todo" @click="select(todo)" />
        </VueDraggable>
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

<script setup lang="ts">
import { computed, inject, onMounted, ref } from 'vue'
import { VueDraggable } from 'vue-draggable-plus'
import * as todosApi from '@/api/todos'
import * as groupsApi from '@/api/groups'
import { useTodosStore } from '@/stores/todos'
import { useUIStore } from '@/stores/ui'
import { useSortableList } from '@/composables/useSortableList'
import FilterBar from '@/components/FilterBar.vue'
import TodoCard from '@/components/TodoCard.vue'
import DetailPanel from '@/components/DetailPanel.vue'
import CreateDialog from '@/components/CreateDialog.vue'
import SkeletonLoader from '@/components/SkeletonLoader.vue'
import EmptyState from '@/components/EmptyState.vue'
import type { Todo } from '@/types/models'

const showToast = inject<(msg: string, type?: 'success' | 'error') => void>('showToast')
const store = useTodosStore()
const ui = useUIStore()
const { onEnd: onReorder } = useSortableList()

const todos = computed(() => store.todos)
const todosProxy = computed<Todo[]>({
  get: () => store.todos,
  set: (val: Todo[]) => {
    store.todos = val
  }
})
const current = ref<Todo | null>(null)
const showCreate = ref(false)
const groups = ref<{ id: number; name: string }[]>([])
const loading = computed(() => store.loading)
const listContainer = ref<HTMLElement | null>(null)

async function load(): Promise<void> {
  try {
    await store.fetch(ui.filter)
  } catch {
    showToast?.('加载失败', 'error')
  }
}

function onFilter(patch: { status?: 'open' | 'closed' | 'archived' | 'all'; priority?: number; group_id?: number; search?: string }): void {
  ui.setFilter(patch)
  load()
}

async function select(todo: Todo): Promise<void> {
  const r = await todosApi.get(todo.id)
  if (r.code === 0) current.value = r.data
}

async function reloadCurrent(): Promise<void> {
  if (current.value) await select(current.value)
  await load()
}

async function onCreated(form: Todo): Promise<void> {
  const r = await todosApi.create(form)
  if (r.code === 0) {
    showToast?.('已创建')
    await load()
    await select(r.data)
  } else {
    showToast?.(r.msg ?? '创建失败', 'error')
  }
}

async function onDragEnd(evt: { oldIndex?: number | undefined; newIndex?: number | undefined }): Promise<void> {
  const { oldIndex, newIndex } = evt
  if (oldIndex === undefined || newIndex === undefined) return
  await onReorder(oldIndex, newIndex)
}

onMounted(async () => {
  const rg = await groupsApi.list()
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
