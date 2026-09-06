import { computed, type ComputedRef, type Ref, unref } from 'vue'
import { useVirtualList } from '@vueuse/core'
import type { Todo } from '@/types/models'

export interface UseVirtualTodosOptions {
  threshold?: number // under this size, render flat
  itemHeight?: number
  overscan?: number
}

export interface UseVirtualTodosApi {
  visible: ComputedRef<Todo[]>
  total: ComputedRef<number>
  usingVirtual: ComputedRef<boolean>
  containerProps: Record<string, unknown>
  wrapperProps: Record<string, unknown>
  itemHeight: number
}

export function useVirtualTodos(
  source: Ref<Todo[]> | ComputedRef<Todo[]>,
  options: UseVirtualTodosOptions = {}
): UseVirtualTodosApi {
  const threshold = options.threshold ?? 100
  const itemHeight = options.itemHeight ?? 120
  const overscan = options.overscan ?? 6

  const total = computed(() => source.value.length)
  const usingVirtual = computed(() => source.value.length > threshold)

  // useVirtualList needs a stable ref, so we forward the original source
  const virtual = useVirtualList(source as Ref<Todo[]>, {
    itemHeight,
    overscan
  })

  const visible = computed<Todo[]>(() => {
    if (!usingVirtual.value) return source.value
    return virtual.list.value
  })

  return {
    visible,
    total,
    usingVirtual,
    containerProps: virtual.containerProps as unknown as Record<string, unknown>,
    wrapperProps: virtual.wrapperProps as unknown as Record<string, unknown>,
    itemHeight
  }
}
