import { useTodosStore } from '@/stores/todos'
import type { Todo } from '@/types/models'

export type KanbanGroupBy = 'priority' | 'group'

export interface KanbanDndApi {
  getColumnItems: (columnKey: number) => Todo[]
  onDrop: (todoId: number, fromColumn: number, toColumn: number, newIndex: number) => Promise<void>
}

export function useKanbanDnd(groupBy: KanbanGroupBy): KanbanDndApi {
  const store = useTodosStore()

  function columnKey(t: Todo): number {
    return groupBy === 'priority' ? t.priority : t.group_id
  }

  function getColumnItems(columnKeyValue: number): Todo[] {
    return store.todos.filter((t) => columnKey(t) === columnKeyValue && t.status === 'open')
  }

  async function onDrop(
    todoId: number,
    fromColumn: number,
    toColumn: number,
    newIndex: number
  ): Promise<void> {
    const todo = store.todos.find((t) => t.id === todoId)
    if (!todo) return

    if (fromColumn === toColumn) {
      const items = getColumnItems(fromColumn)
      const oldIndex = items.findIndex((t) => t.id === todoId)
      if (oldIndex === newIndex || oldIndex < 0) return
      const reordered = [...items]
      const moved = reordered.splice(oldIndex, 1)[0]
      if (!moved) return
      reordered.splice(newIndex, 0, moved)
      const updates = reordered.map((t, idx) => ({ id: t.id, sort_order: idx }))
      await store.reorder(updates)
      return
    }

    // cross-column: change the dimension value
    if (groupBy === 'priority') {
      const updated = await store.update(todoId, { priority: toColumn as Todo['priority'] })
      void updated
    } else {
      const updated = await store.update(todoId, { group_id: toColumn })
      void updated
    }
  }

  return { getColumnItems, onDrop }
}
