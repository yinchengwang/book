/**
 * Domain entity types — single source of truth for the application.
 * Times are unix seconds (matching the existing backend API).
 */

export type TodoStatus = 'open' | 'done' | 'closed' | 'archived'

export type TodoPriority = 0 | 1 | 2 | 3 | 4 // 0 = urgent, 4 = none

export interface ChecklistItem {
  id: number
  text: string
  done: boolean
}

export interface Comment {
  id: number
  text: string
  created_at: number
  user_id?: number
}

export interface Todo {
  id: number
  title: string
  description?: string
  status: TodoStatus
  priority: TodoPriority
  due_date: number // unix seconds, 0 = none
  group_id: number // 0 = ungrouped
  labels: string[] | string // backend may serialize as JSON string
  sort_order?: number
  checklist?: ChecklistItem[]
  comments?: Comment[]
  created_at?: number
  updated_at?: number
}

export interface Group {
  id: number
  name: string
  description?: string
  color?: string
  order?: number
}

export interface User {
  id: number
  email: string
  username: string
  role: 'owner' | 'editor' | 'viewer'
  avatar?: string
  created_at: number
  last_login_at?: number
}

export interface UserPreferences {
  theme: 'light' | 'dark' | 'auto'
  language: string
  notifications: boolean
  default_view: 'list' | 'board' | 'stats'
}

export interface AuthToken {
  access_token: string
  refresh_token?: string
  expires_at: number
}
