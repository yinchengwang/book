import { ref } from 'vue'
import type { Ref } from 'vue'
import { useTodosStore } from '@/stores/todos'

export interface SortableListApi {
  containerRef: Ref<HTMLElement | null>
  onEnd: (oldIndex: number, newIndex: number) => Promise<void>
}

export function useSortableList(): SortableListApi {
  const store = useTodosStore()
  const containerRef = ref<HTMLElement | null>(null)

  async function onEnd(oldIndex: number, newIndex: number): Promise<void> {
    if (oldIndex === newIndex) return
    if (oldIndex < 0 || newIndex < 0) return
    if (oldIndex >= store.todos.length || newIndex >= store.todos.length) return

    const moved = store.todos[oldIndex]
    if (!moved) return

    const next = [...store.todos]
    next.splice(oldIndex, 1)
    next.splice(newIndex, 0, moved)
    store.todos = next

    const updates = next.map((t, idx) => ({ id: t.id, sort_order: idx }))
    try {
      await store.reorder(updates)
    } catch {
      // reorders are best-effort; the optimistic update above is the source of truth
    }
  }

  return { containerRef, onEnd }
}
