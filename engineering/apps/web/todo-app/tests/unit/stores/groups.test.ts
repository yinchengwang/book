import { describe, it, expect, beforeEach, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useGroupsStore } from '@/stores/groups'
import * as groupsApi from '@/api/groups'
import type { Group } from '@/types/models'

vi.mock('@/api/groups', () => ({
  list: vi.fn(),
  get: vi.fn(),
  create: vi.fn(),
  update: vi.fn(),
  remove: vi.fn()
}))

function makeGroup(overrides: Partial<Group> = {}): Group {
  return {
    id: 1,
    name: '测试分组',
    description: '描述',
    color: '#ff0000',
    order: 0,
    ...overrides
  }
}

describe('useGroupsStore', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    vi.clearAllMocks()
  })

  it('starts empty', () => {
    const store = useGroupsStore()
    expect(store.groups).toEqual([])
    expect(store.loading).toBe(false)
  })

  it('fetch loads groups', async () => {
    vi.mocked(groupsApi.list).mockResolvedValue({
      code: 0,
      data: [makeGroup({ id: 1 }), makeGroup({ id: 2, name: '第二组' })]
    })
    const store = useGroupsStore()
    await store.fetch()
    expect(store.groups).toHaveLength(2)
    expect(store.loading).toBe(false)
  })

  it('fetch records error on failure', async () => {
    vi.mocked(groupsApi.list).mockRejectedValue(new Error('network'))
    const store = useGroupsStore()
    await store.fetch()
    expect(store.error).toBeInstanceOf(Error)
    expect(store.error?.message).toBe('network')
  })

  it('byId returns group by id', () => {
    const store = useGroupsStore()
    store.groups = [makeGroup({ id: 1 }), makeGroup({ id: 2 })]
    expect(store.byId(1)?.id).toBe(1)
    expect(store.byId(99)).toBeUndefined()
  })

  it('create appends to list', async () => {
    vi.mocked(groupsApi.create).mockResolvedValue({
      code: 0,
      data: makeGroup({ id: 99, name: '新组' })
    })
    const store = useGroupsStore()
    const result = await store.create({ name: '新组' })
    expect(result.id).toBe(99)
    expect(store.groups).toHaveLength(1)
  })

  it('update replaces matching group', async () => {
    vi.mocked(groupsApi.update).mockResolvedValue({
      code: 0,
      data: makeGroup({ id: 1, name: '更新' })
    })
    const store = useGroupsStore()
    store.groups = [makeGroup({ id: 1, name: '旧' })]
    await store.update(1, { name: '更新' })
    expect(store.groups[0]?.name).toBe('更新')
  })

  it('remove drops from list', async () => {
    vi.mocked(groupsApi.remove).mockResolvedValue({ code: 0 })
    const store = useGroupsStore()
    store.groups = [makeGroup({ id: 1 }), makeGroup({ id: 2 })]
    await store.remove(1)
    expect(store.groups.map((g) => g.id)).toEqual([2])
  })
})
