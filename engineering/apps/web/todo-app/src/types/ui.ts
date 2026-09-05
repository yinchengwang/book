export type Theme = 'light' | 'dark' | 'auto'

export interface ToastMessage {
  msg: string
  type: 'success' | 'error' | 'info'
}

export interface Filter {
  status: 'all' | 'open' | 'closed' | 'archived'
  priority: number // -1 = all
  group_id: number // -1 = all, 0 = ungrouped
  search: string
}

export const DEFAULT_FILTER: Filter = {
  status: 'all',
  priority: -1,
  group_id: -1,
  search: ''
}

export interface ShortcutBinding {
  key: string
  modifiers: Array<'ctrl' | 'meta' | 'alt' | 'shift'>
  description: string
  action: string
}
