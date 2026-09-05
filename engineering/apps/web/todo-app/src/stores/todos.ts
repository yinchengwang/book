import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import * as api from '@/api/todos'
import type { Todo, TodoStatus } from '@/types/models'
import type { CreateTodoInput, UpdateTodoInput, ReorderUpdate, TodoListQuery } from '@/types/api'

export const useTodosStore = defineStore('todos', () => {
  const todos = ref<Todo[]>([])
  const loading = ref(false)
  const error = ref<Error | null>(null)

  const active = computed(() => todos.value.filter((t) => t.status === 'open'))

  const byStatus = computed<Record<TodoStatus, Todo[]>>(() => {
    const buckets: Record<TodoStatus, Todo[]> = {
      open: [],
      done: [],
      closed: [],
      archived: []
    }
    for (const todo of todos.value) {
      buckets[todo.status].push(todo)
    }
    return buckets
  })

  const byGroup = computed<Record<number, Todo[]>>(() => {
    const buckets: Record<number, Todo[]> = {}
    for (const todo of todos.value) {
      const key = todo.group_id
      ;(buckets[key] ||= []).push(todo)
    }
    return buckets
  })

  async function fetch(params: TodoListQuery = {}): Promise<void> {
    loading.value = true
    error.value = null
    try {
      const res = await api.list(params)
      if (res.code === 0 && res.data) {
        todos.value = res.data.items
      } else {
        error.value = new Error(res.msg ?? 'Failed to load todos')
      }
    } catch (e) {
      error.value = e as Error
    } finally {
      loading.value = false
    }
  }

  async function create(input: CreateTodoInput): Promise<Todo> {
    const res = await api.create(input)
    if (res.code !== 0 || !res.data) throw new Error(res.msg ?? 'Create failed')
    todos.value.push(res.data)
    return res.data
  }

  async function update(id: number, input: UpdateTodoInput): Promise<Todo> {
    const res = await api.update(id, input)
    if (res.code !== 0 || !res.data) throw new Error(res.msg ?? 'Update failed')
    const idx = todos.value.findIndex((t) => t.id === id)
    if (idx >= 0) todos.value[idx] = res.data
    return res.data
  }

  async function remove(id: number): Promise<void> {
    const res = await api.remove(id)
    if (res.code !== 0) throw new Error(res.msg ?? 'Remove failed')
    todos.value = todos.value.filter((t) => t.id !== id)
  }

  async function reorder(updates: ReorderUpdate[]): Promise<void> {
    const map = new Map(updates.map((u) => [u.id, u.sort_order]))
    todos.value = [...todos.value]
      .map((t) => ({ ...t, sort_order: map.get(t.id) ?? t.sort_order } as Todo))
      .sort((a, b) => (a.sort_order ?? 0) - (b.sort_order ?? 0))
    await api.reorder(updates)
  }

  async function toggleStatus(id: number): Promise<void> {
    const todo = todos.value.find((t) => t.id === id)
    if (!todo) return
    const next: TodoStatus = todo.status === 'done' ? 'open' : 'done'
    await update(id, { status: next })
  }

  return {
    todos,
    loading,
    error,
    active,
    byStatus,
    byGroup,
    fetch,
    create,
    update,
    remove,
    reorder,
    toggleStatus
  }
})
