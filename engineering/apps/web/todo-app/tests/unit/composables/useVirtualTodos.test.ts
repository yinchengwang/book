import { describe, it, expect, vi } from 'vitest'
import { ref, computed } from 'vue'
import { useVirtualTodos } from '@/composables/useVirtualTodos'
import type { Todo } from '@/types/models'

// Mock useVirtualList to test our composable logic in isolation
vi.mock('@vueuse/core', () => ({
  useVirtualList: vi.fn((source, options) => {
    const list = source.value || []
    const itemHeight = options.itemHeight || 120
    const capacity = 10 // Simulate showing 10 items at a time

    return {
      list: { value: list.slice(0, capacity) },
      scrollTo: vi.fn(),
      containerProps: {
        style: {
          height: '500px',
          overflowY: 'auto'
        }
      },
      wrapperProps: { value: { style: { height: `${list.length * itemHeight}px` } } }
    }
  })
}))

function makeTodos(n: number): Todo[] {
  return Array.from({ length: n }, (_, i) => ({
    id: i + 1,
    title: `task ${i + 1}`,
    status: 'open',
    priority: 2,
    due_date: 0,
    group_id: 0,
    labels: []
  }))
}

describe('useVirtualTodos', () => {
  it('returns all items when fewer than threshold', () => {
    const list = ref(makeTodos(20))
    const { visible, total, usingVirtual } = useVirtualTodos(list, { threshold: 50 })
    expect(usingVirtual.value).toBe(false)
    expect(visible.value).toHaveLength(20)
    expect(total.value).toBe(20)
  })

  it('returns a window when more than threshold', () => {
    const list = ref(makeTodos(500))
    const { visible, total, usingVirtual } = useVirtualTodos(list, { threshold: 50, itemHeight: 120 })
    expect(usingVirtual.value).toBe(true)
    expect(total.value).toBe(500)
    expect(visible.value.length).toBeGreaterThan(0)
    expect(visible.value.length).toBeLessThan(500)
  })

  it('containerProps and wrapperProps expose correct values', () => {
    const list = ref(makeTodos(500))
    const { containerProps, wrapperProps, itemHeight } = useVirtualTodos(list, { threshold: 50, itemHeight: 120 })
    const container = containerProps as unknown as { style: { height: string; overflowY: string } }
    const wrapper = wrapperProps as unknown as { value: { style: { height: string } } }
    expect(container.style.overflowY).toBe('auto')
    expect(parseInt(String(container.style.height), 10)).toBeGreaterThan(0)
    expect(parseInt(String(wrapper.value.style.height), 10)).toBe(120 * 500)
    expect(itemHeight).toBe(120)
  })
})
