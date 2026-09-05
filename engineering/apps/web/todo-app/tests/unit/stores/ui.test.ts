import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useUIStore } from '@/stores/ui'
import { DEFAULT_FILTER } from '@/types/ui'

describe('useUIStore', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
  })

  it('starts with default filter, auto theme, and empty toasts', () => {
    const store = useUIStore()
    expect(store.filter).toEqual(DEFAULT_FILTER)
    expect(store.theme).toBe('auto')
    expect(store.toasts).toEqual([])
  })

  it('setFilter merges partial filter values', () => {
    const store = useUIStore()
    store.setFilter({ status: 'open', search: 'foo' })
    expect(store.filter.status).toBe('open')
    expect(store.filter.search).toBe('foo')
    expect(store.filter.priority).toBe(DEFAULT_FILTER.priority)
    expect(store.filter.group_id).toBe(DEFAULT_FILTER.group_id)
  })

  it('setFilter can update group_id and priority', () => {
    const store = useUIStore()
    store.setFilter({ group_id: 7, priority: 2 })
    expect(store.filter.group_id).toBe(7)
    expect(store.filter.priority).toBe(2)
  })

  it('resetFilter restores DEFAULT_FILTER', () => {
    const store = useUIStore()
    store.setFilter({ status: 'closed', priority: 3, group_id: 5, search: 'q' })
    store.resetFilter()
    expect(store.filter).toEqual(DEFAULT_FILTER)
  })

  it('setTheme changes the theme value', () => {
    const store = useUIStore()
    store.setTheme('dark')
    expect(store.theme).toBe('dark')
    store.setTheme('light')
    expect(store.theme).toBe('light')
  })

  it('pushToast appends a toast and returns its id', () => {
    const store = useUIStore()
    const id = store.pushToast({ msg: '保存成功', type: 'success' })
    expect(typeof id).toBe('number')
    expect(store.toasts).toHaveLength(1)
    expect(store.toasts[0]?.msg).toBe('保存成功')
    expect(store.toasts[0]?.type).toBe('success')
    expect(store.toasts[0]?.id).toBe(id)
  })

  it('pushToast assigns unique ids across calls', () => {
    const store = useUIStore()
    const a = store.pushToast({ msg: 'first', type: 'info' })
    const b = store.pushToast({ msg: 'second', type: 'error' })
    expect(a).not.toBe(b)
    expect(store.toasts).toHaveLength(2)
  })

  it('dismissToast removes the matching toast by id', () => {
    const store = useUIStore()
    const a = store.pushToast({ msg: 'keep', type: 'info' })
    const b = store.pushToast({ msg: 'drop', type: 'error' })
    store.dismissToast(a)
    expect(store.toasts.map((t) => t.id)).toEqual([b])
    expect(store.toasts[0]?.msg).toBe('drop')
  })

  it('dismissToast on unknown id is a no-op', () => {
    const store = useUIStore()
    const a = store.pushToast({ msg: 'keep', type: 'info' })
    store.dismissToast(9999)
    expect(store.toasts.map((t) => t.id)).toEqual([a])
  })
})