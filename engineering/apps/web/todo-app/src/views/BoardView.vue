<template>
  <div class="board-view">
    <div class="board-controls">
      <select v-model="groupBy" class="form-select" data-test="group-by">
        <option value="priority">按优先级</option>
        <option value="group">按分组</option>
      </select>
      <button class="btn btn-primary btn-sm" @click="showCreate = true">+ 新建</button>
    </div>
    <div class="board-columns">
      <div v-for="col in columns" :key="col.key" class="board-column">
        <div class="column-header">
          <span>{{ col.name }}</span>
          <span class="column-count">{{ itemsFor(col.key).length }}</span>
        </div>
        <VueDraggable
          :model-value="itemsFor(col.key)"
          :animation="180"
          group="kanban"
          :data-column="col.key"
          class="column-todos"
          item-key="id"
          @add="onAdd($event, col.key)"
          @update="onReorder($event, col.key)"
        >
          <TodoCard
            v-for="todo in itemsFor(col.key)"
            :key="todo.id"
            :todo="todo"
            @click="select(todo)"
          />
        </VueDraggable>
        <div v-if="itemsFor(col.key).length === 0" class="column-empty">无待办</div>
      </div>
    </div>
    <DetailPanel v-if="current" :todo="current" @updated="reloadCurrent" @close="current = null" />
    <CreateDialog v-model="showCreate" :groups="groups" @created="onCreated" />
  </div>
</template>

<script setup lang="ts">
import { computed, inject, onMounted, ref } from 'vue'
import { VueDraggable } from 'vue-draggable-plus'
import api from '@/api.js'
import { useTodosStore } from '@/stores/todos'
import { useGroupsStore } from '@/stores/groups'
import { useKanbanDnd } from '@/composables/useKanbanDnd'
import TodoCard from '@/components/TodoCard.vue'
import DetailPanel from '@/components/DetailPanel.vue'
import CreateDialog from '@/components/CreateDialog.vue'
import type { Todo } from '@/types/models'

const showToast = inject<(msg: string, type?: 'success' | 'error') => void>('showToast')
const todosStore = useTodosStore()
const groupsStore = useGroupsStore()
const groupBy = ref<'priority' | 'group'>('priority')
const showCreate = ref(false)
const current = ref<Todo | null>(null)
const groups = computed(() => groupsStore.groups)

const dnd = useKanbanDnd(groupBy.value)

const PRIORITY_NAMES = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']

const columns = computed(() => {
  if (groupBy.value === 'priority') {
    return PRIORITY_NAMES.map((name, i) => ({ key: i, name }))
  }
  const cols = [{ key: 0, name: '未分组' }]
  for (const g of groupsStore.groups) cols.push({ key: g.id, name: g.name })
  return cols
})

function itemsFor(key: number): Todo[] {
  return dnd.getColumnItems(key)
}

async function loadData(): Promise<void> {
  await Promise.all([todosStore.fetch({ status: 'all' }), groupsStore.fetch()])
}

async function select(todo: Todo): Promise<void> {
  const r = await api.get(todo.id)
  if (r.code === 0) current.value = r.data
}

async function reloadCurrent(): Promise<void> {
  if (current.value) await select(current.value)
  await loadData()
}

async function onCreated(form: Todo): Promise<void> {
  const r = await api.create(form)
  if (r.code === 0) {
    showToast?.('已创建')
    await loadData()
  } else {
    showToast?.(r.msg ?? '创建失败', 'error')
  }
}

async function onAdd(evt: { item?: HTMLElement; newIndex?: number | undefined }, toColumn: number): Promise<void> {
  const el = evt.item
  const newIndex = evt.newIndex ?? 0
  if (!el) return
  const id = Number(el.dataset.id)
  if (!id) return
  const fromColumn = Number(el.dataset.column ?? toColumn)
  await dnd.onDrop(id, fromColumn, toColumn, newIndex)
}

async function onReorder(evt: { oldIndex?: number | undefined; newIndex?: number | undefined }, columnKey: number): Promise<void> {
  if (evt.oldIndex === undefined || evt.newIndex === undefined) return
  const items = itemsFor(columnKey)
  const todo = items[evt.oldIndex]
  if (!todo) return
  await dnd.onDrop(todo.id, columnKey, columnKey, evt.newIndex)
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
.column-todos { display: flex; flex-direction: column; gap: 8px; min-height: 40px; }
.column-empty { text-align: center; color: var(--text-muted); font-size: 12px; padding: 20px 0; border: 2px dashed var(--border); border-radius: var(--radius); }
</style>
