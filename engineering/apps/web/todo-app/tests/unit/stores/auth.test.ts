import { describe, it, expect, beforeEach, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useAuthStore } from '@/stores/auth'
import * as authApi from '@/api/auth'
import type { User } from '@/types/models'

vi.mock('@/api/auth', () => ({
  login: vi.fn(),
  register: vi.fn(),
  logout: vi.fn(),
  me: vi.fn()
}))

const fakeUser: User = {
  id: 1,
  email: 'a@b.com',
  username: 'alice',
  role: 'editor',
  created_at: 0
}

describe('useAuthStore', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    localStorage.clear()
    vi.clearAllMocks()
  })

  it('starts unauthenticated', () => {
    const store = useAuthStore()
    expect(store.isAuthenticated).toBe(false)
    expect(store.user).toBeNull()
  })

  it('login sets user and token', async () => {
    vi.mocked(authApi.login).mockResolvedValue({ user: fakeUser, access_token: 'tok-123', expires_at: Date.now() + 3600_000 })
    const store = useAuthStore()
    await store.login({ email: 'a@b.com', password: 'pw' })
    expect(store.user).toEqual(fakeUser)
    expect(store.token?.access_token).toBe('tok-123')
    expect(store.isAuthenticated).toBe(true)
  })

  it('register creates user', async () => {
    vi.mocked(authApi.register).mockResolvedValue({ user: fakeUser, access_token: 'tok-xyz', expires_at: Date.now() + 3600_000 })
    const store = useAuthStore()
    await store.register({ email: 'a@b.com', username: 'alice', password: 'pw' })
    expect(store.user?.email).toBe('a@b.com')
  })

  it('logout clears user and token', async () => {
    vi.mocked(authApi.logout).mockResolvedValue(undefined)
    const store = useAuthStore()
    store.user = fakeUser
    store.token = { access_token: 'x', expires_at: Date.now() + 3600_000 }
    await store.logout()
    expect(store.user).toBeNull()
    expect(store.token).toBeNull()
  })

  it('loadFromStorage hydrates from localStorage', () => {
    localStorage.setItem('auth', JSON.stringify({ user: fakeUser, token: { access_token: 'x', expires_at: Date.now() + 3600_000 } }))
    const store = useAuthStore()
    store.loadFromStorage()
    expect(store.user?.id).toBe(1)
    expect(store.isAuthenticated).toBe(true)
  })

  it('loadFromStorage ignores corrupted data', () => {
    localStorage.setItem('auth', 'not-json')
    const store = useAuthStore()
    expect(() => store.loadFromStorage()).not.toThrow()
    expect(store.user).toBeNull()
  })

  it('throws when login fails', async () => {
    vi.mocked(authApi.login).mockRejectedValue(new Error('invalid'))
    const store = useAuthStore()
    await expect(store.login({ email: 'a@b.com', password: 'wrong' })).rejects.toThrow('invalid')
    expect(store.user).toBeNull()
  })
})
