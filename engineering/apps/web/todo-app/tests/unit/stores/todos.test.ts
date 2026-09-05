import { describe, it, expect, beforeEach, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useTodosStore } from '@/stores/todos'
import * as todosApi from '@/api/todos'
import type { Todo } from '@/types/models'

vi.mock('@/api/todos', () => ({
  list: vi.fn(),
  get: vi.fn(),
  create: vi.fn(),
  update: vi.fn(),
  remove: vi.fn(),
  updateSort: vi.fn(),
  reorder: vi.fn()
}))

function makeTodo(overrides: Partial<Todo> = {}): Todo {
  return {
    id: 1,
    title: '测试任务',
    status: 'open',
    priority: 2,
    due_date: 0,
    group_id: 0,
    labels: [],
    ...overrides
  }
}

describe('useTodosStore', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    vi.clearAllMocks()
  })

  it('starts empty', () => {
    const store = useTodosStore()
    expect(store.todos).toEqual([])
    expect(store.loading).toBe(false)
    expect(store.error).toBeNull()
  })

  it('fetch loads todos', async () => {
    vi.mocked(todosApi.list).mockResolvedValue({
      code: 0,
      data: { items: [makeTodo({ id: 1 }), makeTodo({ id: 2, status: 'done' })] }
    })
    const store = useTodosStore()
    await store.fetch({ status: 'all' })
    expect(store.todos).toHaveLength(2)
    expect(store.loading).toBe(false)
  })

  it('fetch records error on failure', async () => {
    vi.mocked(todosApi.list).mockRejectedValue(new Error('network'))
    const store = useTodosStore()
    await store.fetch({ status: 'all' })
    expect(store.error).toBeInstanceOf(Error)
    expect(store.error?.message).toBe('network')
  })

  it('active excludes done and closed', () => {
    const store = useTodosStore()
    store.todos = [
      makeTodo({ id: 1, status: 'open' }),
      makeTodo({ id: 2, status: 'done' }),
      makeTodo({ id: 3, status: 'closed' }),
      makeTodo({ id: 4, status: 'open' })
    ]
    expect(store.active.map((t) => t.id)).toEqual([1, 4])
  })

  it('byStatus groups todos by status', () => {
    const store = useTodosStore()
    store.todos = [
      makeTodo({ id: 1, status: 'open' }),
      makeTodo({ id: 2, status: 'done' }),
      makeTodo({ id: 3, status: 'open' })
    ]
    expect(store.byStatus.open).toHaveLength(2)
    expect(store.byStatus.done).toHaveLength(1)
    expect(store.byStatus.archived).toHaveLength(0)
  })

  it('byGroup buckets by group_id', () => {
    const store = useTodosStore()
    store.todos = [
      makeTodo({ id: 1, group_id: 0 }),
      makeTodo({ id: 2, group_id: 5 }),
      makeTodo({ id: 3, group_id: 5 })
    ]
    expect(store.byGroup[0]).toHaveLength(1)
    expect(store.byGroup[5]).toHaveLength(2)
  })

  it('create appends to list', async () => {
    vi.mocked(todosApi.create).mockResolvedValue({
      code: 0,
      data: makeTodo({ id: 99, title: 'new' })
    })
    const store = useTodosStore()
    const result = await store.create({ title: 'new', priority: 2, due_date: 0, group_id: 0, labels: [] })
    expect(result.id).toBe(99)
    expect(store.todos).toHaveLength(1)
  })

  it('update replaces matching todo', async () => {
    vi.mocked(todosApi.update).mockResolvedValue({
      code: 0,
      data: makeTodo({ id: 1, title: 'updated' })
    })
    const store = useTodosStore()
    store.todos = [makeTodo({ id: 1, title: 'old' })]
    await store.update(1, { title: 'updated' })
    expect(store.todos[0]?.title).toBe('updated')
  })

  it('remove drops from list', async () => {
    vi.mocked(todosApi.remove).mockResolvedValue({ code: 0 })
    const store = useTodosStore()
    store.todos = [makeTodo({ id: 1 }), makeTodo({ id: 2 })]
    await store.remove(1)
    expect(store.todos.map((t) => t.id)).toEqual([2])
  })

  it('reorder updates sort_order', async () => {
    vi.mocked(todosApi.updateSort).mockResolvedValue({ code: 0 })
    const store = useTodosStore()
    store.todos = [makeTodo({ id: 1 }), makeTodo({ id: 2 })]
    await store.reorder([{ id: 1, sort_order: 5 }, { id: 2, sort_order: 1 }])
    expect(store.todos[0]?.id).toBe(2)
    expect(store.todos[1]?.id).toBe(1)
  })

  it('toggleStatus flips open/done', async () => {
    vi.mocked(todosApi.update).mockResolvedValue({
      code: 0,
      data: makeTodo({ id: 1, status: 'done' })
    })
    const store = useTodosStore()
    store.todos = [makeTodo({ id: 1, status: 'open' })]
    await store.toggleStatus(1)
    expect(store.todos[0]?.status).toBe('done')
  })
})
