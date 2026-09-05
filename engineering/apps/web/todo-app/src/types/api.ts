import type { Todo, Group } from './models'

/** Envelope returned by every backend endpoint. */
export interface ApiResponse<T> {
  code: number // 0 = success, non-zero = error
  data?: T
  msg?: string
  meta?: PaginationMeta
}

export interface PaginationMeta {
  page: number
  per_page: number
  total: number
}

export interface TodoListQuery {
  status?: 'all' | 'open' | 'closed' | 'archived'
  priority?: number // -1 = all
  group_id?: number // -1 = all, 0 = ungrouped
  search?: string
  page?: number
  per_page?: number
}

export interface TodoListResult {
  items: Todo[]
}

export interface CreateTodoInput {
  title: string
  description?: string
  priority: number
  due_date: number
  group_id: number
  labels: string[]
}

export interface UpdateTodoInput {
  title?: string
  description?: string
  priority?: number
  due_date?: number
  group_id?: number
  labels?: string[]
  status?: Todo['status']
}

export interface ReorderUpdate {
  id: number
  sort_order: number
}

export interface CreateGroupInput {
  name: string
  description?: string
}

export type GroupListResult = Group[]
