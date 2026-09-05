import type { ApiResponse, GroupListResult, CreateGroupInput } from '@/types/api'
import type { Group } from '@/types/models'

const BASE = '/api'

async function request<T>(input: string, init?: RequestInit): Promise<ApiResponse<T>> {
  const res = await fetch(`${BASE}${input}`, init)
  return (await res.json()) as ApiResponse<T>
}

export function list(): Promise<ApiResponse<GroupListResult>> {
  return request<GroupListResult>('/groups')
}

export function get(id: number): Promise<ApiResponse<Group>> {
  return request<Group>(`/groups/${id}`)
}

export function create(body: CreateGroupInput): Promise<ApiResponse<Group>> {
  return request<Group>('/groups', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  })
}

export function update(id: number, body: Partial<CreateGroupInput>): Promise<ApiResponse<Group>> {
  return request<Group>(`/groups/${id}`, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  })
}

export function remove(id: number): Promise<ApiResponse<null>> {
  return request<null>(`/groups/${id}`, { method: 'DELETE' })
}
