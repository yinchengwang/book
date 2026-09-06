<template>
  <div id="app">
    <nav class="nav">
      <router-link to="/" class="nav-link">📋 列表</router-link>
      <router-link to="/board" class="nav-link">📊 看板</router-link>
      <router-link to="/stats" class="nav-link">📈 统计</router-link>
      <router-link to="/groups" class="nav-link">🏷️ 分组</router-link>
      <span class="spacer"></span>
      <ThemeToggle />
      <button v-if="auth.isAuthenticated" class="nav-link" @click="logout">退出 ({{ auth.user?.username }})</button>
      <router-link v-else to="/login" class="nav-link">登录</router-link>
    </nav>
    <router-view />
    <div class="toast-stack" aria-live="polite">
      <div
        v-for="t in ui.toasts"
        :key="t.id"
        :class="['toast', 'toast-' + t.type]"
      >{{ t.msg }}</div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useUIStore } from '@/stores/ui'
import { useShortcuts } from '@/composables/useShortcuts'
import ThemeToggle from '@/components/common/ThemeToggle.vue'

const router = useRouter()
const auth = useAuthStore()
const ui = useUIStore()
const { addBinding } = useShortcuts()

async function logout(): Promise<void> {
  await auth.logout()
  router.push('/login')
}

onMounted(() => {
  addBinding({ key: 'n', modifiers: [], description: '新建任务', action: 'newTodo' }, () => {
    const btn = document.querySelector<HTMLButtonElement>('.btn-primary.btn-sm')
    btn?.click()
  })
  addBinding({ key: '/', modifiers: [], description: '聚焦搜索', action: 'focusSearch' }, () => {
    const input = document.querySelector<HTMLInputElement>('.search-input')
    input?.focus()
  })
})

onUnmounted(() => {})
</script>

<style>
.toast-stack {
  position: fixed;
  bottom: 16px;
  right: 16px;
  display: flex;
  flex-direction: column;
  gap: 8px;
  z-index: 999;
}
.toast {
  padding: 8px 12px;
  border-radius: 6px;
  color: white;
  min-width: 160px;
  animation: toastIn 0.2s ease-out;
}
.toast-success { background: var(--success); }
.toast-error { background: var(--danger); }
.toast-info { background: var(--primary); }
@keyframes toastIn {
  from { opacity: 0; transform: translateX(20px); }
  to { opacity: 1; transform: translateX(0); }
}
.nav .spacer { flex: 1; }
</style>
