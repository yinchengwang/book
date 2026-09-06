import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import * as api from '@/api/auth'
import type { User, AuthToken } from '@/types/models'

interface PersistedAuth {
  user: User | null
  token: AuthToken | null
}

export const useAuthStore = defineStore('auth', () => {
  const user = ref<User | null>(null)
  const token = ref<AuthToken | null>(null)
  const loading = ref(false)

  const isAuthenticated = computed(() => !!user.value && !!token.value)

  async function login(input: { email: string; password: string }): Promise<void> {
    loading.value = true
    try {
      const payload = await api.login(input)
      user.value = payload.user
      token.value = {
        access_token: payload.access_token,
        refresh_token: payload.refresh_token,
        expires_at: payload.expires_at
      }
    } finally {
      loading.value = false
    }
  }

  async function register(input: { email: string; username: string; password: string }): Promise<void> {
    loading.value = true
    try {
      const payload = await api.register(input)
      user.value = payload.user
      token.value = {
        access_token: payload.access_token,
        refresh_token: payload.refresh_token,
        expires_at: payload.expires_at
      }
    } finally {
      loading.value = false
    }
  }

  async function logout(): Promise<void> {
    try {
      await api.logout()
    } finally {
      user.value = null
      token.value = null
    }
  }

  function loadFromStorage(): void {
    const raw = localStorage.getItem('auth')
    if (!raw) return
    try {
      const parsed = JSON.parse(raw) as PersistedAuth
      user.value = parsed.user
      token.value = parsed.token
    } catch {
      // corrupted — ignore and remain logged out
    }
  }

  return {
    user,
    token,
    loading,
    isAuthenticated,
    login,
    register,
    logout,
    loadFromStorage
  }
})
