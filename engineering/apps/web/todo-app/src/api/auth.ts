import type { AuthToken, User } from '@/types/models'

const BASE = '/api'

async function post<T>(path: string, body: unknown): Promise<T> {
  const res = await fetch(`${BASE}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  })
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return (await res.json()) as T
}

export interface AuthPayload {
  user: User
  access_token: string
  refresh_token?: string
  expires_at: number
}

export function login(input: { email: string; password: string }): Promise<AuthPayload> {
  return post<AuthPayload>('/auth/login', input)
}

export function register(input: { email: string; username: string; password: string }): Promise<AuthPayload> {
  return post<AuthPayload>('/auth/register', input)
}

export async function logout(): Promise<void> {
  await post<{ success: true }>('/auth/logout', {})
}

export async function me(token: string): Promise<User> {
  const res = await fetch(`${BASE}/users/me`, { headers: { Authorization: `Bearer ${token}` } })
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return (await res.json()) as User
}

export type { AuthToken }
