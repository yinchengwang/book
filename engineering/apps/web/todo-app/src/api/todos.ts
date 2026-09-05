import type { ApiResponse, TodoListQuery, CreateTodoInput, UpdateTodoInput, ReorderUpdate, TodoListResult } from '@/types/api'
import type { Todo } from '@/types/models'

const BASE = '/api'

async function request<T>(input: string, init?: RequestInit): Promise<ApiResponse<T>> {
  const res = await fetch(`${BASE}${input}`, init)
  return (await res.json()) as ApiResponse<T>
}

export function list(params: TodoListQuery = {}): Promise<ApiResponse<TodoListResult>> {
  const search = new URLSearchParams()
  if (params.status) search.set('status', params.status)
  if (params.priority !== undefined) search.set('priority', String(params.priority))
  if (params.group_id !== undefined) search.set('group_id', String(params.group_id))
  if (params.search) search.set('search', params.search)
  if (params.page) search.set('page', String(params.page))
  if (params.per_page) search.set('per_page', String(params.per_page))
  const qs = search.toString()
  return request<TodoListResult>(`/todos${qs ? `?${qs}` : ''}`)
}

export function get(id: number): Promise<ApiResponse<Todo>> {
  return request<Todo>(`/todos/${id}`)
}

export function create(body: CreateTodoInput): Promise<ApiResponse<Todo>> {
  return request<Todo>('/todos', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  })
}

export function update(id: number, body: UpdateTodoInput): Promise<ApiResponse<Todo>> {
  return request<Todo>(`/todos/${id}`, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  })
}

export function remove(id: number): Promise<ApiResponse<null>> {
  return request<null>(`/todos/${id}`, { method: 'DELETE' })
}

export function updateSort(id: number, sort_order: number): Promise<ApiResponse<null>> {
  return request<null>(`/todos/${id}/sort`, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ sort_order })
  })
}

export async function reorder(updates: ReorderUpdate[]): Promise<void> {
  await Promise.all(updates.map((u) => updateSort(u.id, u.sort_order)))
}
