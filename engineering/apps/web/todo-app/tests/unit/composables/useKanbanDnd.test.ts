import { describe, it, expect, beforeEach, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useKanbanDnd } from '@/composables/useKanbanDnd'
import { useTodosStore } from '@/stores/todos'
import * as todosApi from '@/api/todos'
import type { Todo } from '@/types/models'

vi.mock('@/api/todos', () => ({
  update: vi.fn(),
  list: vi.fn(),
  get: vi.fn(),
  create: vi.fn(),
  remove: vi.fn(),
  updateSort: vi.fn(),
  reorder: vi.fn()
}))

function t(id: number, overrides: Partial<Todo> = {}): Todo {
  return {
    id,
    title: `t${id}`,
    status: 'open',
    priority: 2,
    due_date: 0,
    group_id: 0,
    labels: [],
    ...overrides
  }
}

describe('useKanbanDnd', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    vi.clearAllMocks()
  })

  it('按 priority 列分组时按优先级列分组', () => {
    const store = useTodosStore()
    store.todos = [t(1, { priority: 0 }), t(2, { priority: 2 }), t(3, { priority: 2 })]
    const { getColumnItems } = useKanbanDnd('priority')
    expect(getColumnItems(0).map((x) => x.id)).toEqual([1])
    expect(getColumnItems(2).map((x) => x.id)).toEqual([2, 3])
  })

  it('按 group 列分组时按 group_id 分组', () => {
    const store = useTodosStore()
    store.todos = [t(1, { group_id: 0 }), t(2, { group_id: 5 })]
    const { getColumnItems } = useKanbanDnd('group')
    expect(getColumnItems(0).map((x) => x.id)).toEqual([1])
    expect(getColumnItems(5).map((x) => x.id)).toEqual([2])
  })

  it('同列内 onDrop 重新排序', async () => {
    const store = useTodosStore()
    store.todos = [t(1, { priority: 2 }), t(2, { priority: 2 })]
    vi.mocked(todosApi.reorder).mockResolvedValue(undefined as any)
    const { onDrop } = useKanbanDnd('priority')
    // Move todo 2 (currently at index 1) to index 0
    await onDrop(2, 2, 2, 0)
    expect(store.todos.map((x) => x.id)).toEqual([2, 1])
  })

  it('跨列 onDrop 更新 priority', async () => {
    const store = useTodosStore()
    store.todos = [t(1, { priority: 2 })]
    vi.mocked(todosApi.update).mockResolvedValue({ code: 0, data: t(1, { priority: 0 }) })
    const { onDrop } = useKanbanDnd('priority')
    await onDrop(1, 2, 0, 0)
    expect(store.todos[0]?.priority).toBe(0)
  })

  it('跨列 onDrop 更新 group_id', async () => {
    const store = useTodosStore()
    store.todos = [t(1, { group_id: 0 })]
    vi.mocked(todosApi.update).mockResolvedValue({ code: 0, data: t(1, { group_id: 7 }) })
    const { onDrop } = useKanbanDnd('group')
    await onDrop(1, 0, 7, 0)
    expect(store.todos[0]?.group_id).toBe(7)
  })

  it('onDrop 对未知 todo 不执行操作', async () => {
    const store = useTodosStore()
    store.todos = [t(1, { priority: 2 })]
    const { onDrop } = useKanbanDnd('priority')
    await onDrop(99, 2, 2, 0)
    expect(store.todos[0]?.priority).toBe(2)
  })
})