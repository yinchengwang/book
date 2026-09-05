import { defineStore } from 'pinia'
import { ref } from 'vue'
import { DEFAULT_FILTER } from '@/types/ui'
import type { Filter, Theme, ToastMessage } from '@/types/ui'

interface ToastEntry extends ToastMessage {
  id: number
}

let nextToastId = 1

export const useUIStore = defineStore('ui', () => {
  const filter = ref<Filter>({ ...DEFAULT_FILTER })
  const theme = ref<Theme>('auto')
  const toasts = ref<ToastEntry[]>([])

  function setFilter(partial: Partial<Filter>): void {
    filter.value = { ...filter.value, ...partial }
  }

  function resetFilter(): void {
    filter.value = { ...DEFAULT_FILTER }
  }

  function setTheme(value: Theme): void {
    theme.value = value
  }

  function pushToast(toast: ToastMessage): number {
    const entry: ToastEntry = { ...toast, id: nextToastId++ }
    toasts.value.push(entry)
    return entry.id
  }

  function dismissToast(id: number): void {
    toasts.value = toasts.value.filter((t) => t.id !== id)
  }

  return {
    filter,
    theme,
    toasts,
    setFilter,
    resetFilter,
    setTheme,
    pushToast,
    dismissToast
  }
})