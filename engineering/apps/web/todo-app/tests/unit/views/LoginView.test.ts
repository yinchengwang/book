import { describe, it, expect, beforeEach, vi } from 'vitest'
import { mount, flushPromises } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import { createRouter, createMemoryHistory } from 'vue-router'
import LoginView from '@/views/LoginView.vue'
import { useAuthStore } from '@/stores/auth'
import * as authApi from '@/api/auth'

vi.mock('@/api/auth', () => ({
  login: vi.fn(),
  register: vi.fn(),
  logout: vi.fn(),
  me: vi.fn()
}))

function makeRouter() {
  return createRouter({
    history: createMemoryHistory(),
    routes: [
      { path: '/', component: { template: '<div/>' } },
      { path: '/login', component: LoginView },
      { path: '/register', component: { template: '<div/>' } }
    ]
  })
}

describe('LoginView', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    vi.clearAllMocks()
  })

  it('renders email and password inputs', async () => {
    const router = makeRouter()
    await router.push('/login')
    await router.isReady()
    const wrapper = mount(LoginView, {
      global: { plugins: [router] }
    })
    expect(wrapper.find('[data-test="email"]').exists()).toBe(true)
    expect(wrapper.find('[data-test="password"]').exists()).toBe(true)
    expect(wrapper.find('[data-test="submit"]').exists()).toBe(true)
  })

  it('disables submit when fields are empty', async () => {
    const router = makeRouter()
    await router.push('/login')
    await router.isReady()
    const wrapper = mount(LoginView, { global: { plugins: [router] } })
    const submit = wrapper.find('[data-test="submit"]')
    expect((submit.element as HTMLButtonElement).disabled).toBe(true)
  })

  it('logs in and navigates to / on success', async () => {
    vi.mocked(authApi.login).mockResolvedValue({
      user: { id: 1, email: 'a@b.com', username: 'a', role: 'editor', created_at: 0 },
      access_token: 'tok',
      expires_at: Date.now() + 3600_000
    })
    const router = makeRouter()
    await router.push('/login')
    await router.isReady()
    const wrapper = mount(LoginView, { global: { plugins: [router] } })

    await wrapper.find('[data-test="email"]').setValue('a@b.com')
    await wrapper.find('[data-test="password"]').setValue('secret')
    await wrapper.find('form').trigger('submit')

    await flushPromises()
    await flushPromises()

    expect(useAuthStore().isAuthenticated).toBe(true)
    expect(router.currentRoute.value.path).toBe('/')
  })

  it('shows error on failure', async () => {
    vi.mocked(authApi.login).mockRejectedValue(new Error('invalid'))
    const router = makeRouter()
    await router.push('/login')
    await router.isReady()
    const wrapper = mount(LoginView, { global: { plugins: [router] } })

    await wrapper.find('[data-test="email"]').setValue('a@b.com')
    await wrapper.find('[data-test="password"]').setValue('wrong')
    await wrapper.find('form').trigger('submit')

    await flushPromises()
    await flushPromises()
    await wrapper.vm.$nextTick()

    expect(wrapper.text()).toContain('invalid')
  })
})
