import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useSortableList } from '@/composables/useSortableList'
import { useTodosStore } from '@/stores/todos'
import type { Todo } from '@/types/models'

function makeTodo(id: number, sort_order: number): Todo {
  return {
    id,
    title: `t${id}`,
    status: 'open',
    priority: 2,
    due_date: 0,
    group_id: 0,
    labels: [],
    sort_order
  }
}

describe('useSortableList', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
  })

  it('returns a containerRef and onEnd', () => {
    const { containerRef, onEnd } = useSortableList()
    expect(containerRef.value).toBeNull()
    expect(typeof onEnd).toBe('function')
  })

  it('reorders the store on drag end', async () => {
    const store = useTodosStore()
    store.todos = [makeTodo(1, 0), makeTodo(2, 1), makeTodo(3, 2)]
    const { onEnd } = useSortableList()
    await onEnd(0, 2)
    expect(store.todos.map((t) => t.id)).toEqual([2, 3, 1])
  })

  it('commits sort_order values monotonically increasing', async () => {
    const store = useTodosStore()
    store.todos = [makeTodo(1, 0), makeTodo(2, 1), makeTodo(3, 2)]
    const { onEnd } = useSortableList()
    await onEnd(2, 0)
    const orders = store.todos.map((t) => t.sort_order ?? 0)
    expect(orders).toEqual([0, 1, 2])
  })

  it('does nothing when indices are equal', async () => {
    const store = useTodosStore()
    store.todos = [makeTodo(1, 5), makeTodo(2, 10)]
    const { onEnd } = useSortableList()
    await onEnd(1, 1)
    expect(store.todos.map((t) => t.id)).toEqual([1, 2])
  })

  it('ignores reorder when todos have no ids', async () => {
    const store = useTodosStore()
    store.todos = [
      { id: 1, title: 'a', status: 'open', priority: 2, due_date: 0, group_id: 0, labels: [] } as Todo
    ]
    const { onEnd } = useSortableList()
    await expect(onEnd(0, 0)).resolves.toBeUndefined()
  })
})
