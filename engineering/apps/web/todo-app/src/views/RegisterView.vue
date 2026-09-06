<template>
  <div class="auth-page">
    <form class="auth-card" @submit.prevent="submit">
      <h2>注册</h2>
      <label class="label-text">邮箱</label>
      <input
        v-model.trim="email"
        type="email"
        class="form-input"
        autocomplete="email"
        required
        data-test="email"
      />
      <label class="label-text">用户名</label>
      <input
        v-model.trim="username"
        class="form-input"
        autocomplete="username"
        minlength="3"
        required
        data-test="username"
      />
      <label class="label-text">密码（至少 8 位）</label>
      <input
        v-model="password"
        type="password"
        class="form-input"
        autocomplete="new-password"
        minlength="8"
        required
        data-test="password"
      />
      <p v-if="error" class="auth-error" data-test="error">{{ error }}</p>
      <button
        type="submit"
        class="btn btn-primary"
        :disabled="!canSubmit || loading"
        data-test="submit"
      >{{ loading ? '注册中...' : '注册' }}</button>
      <p class="auth-link">
        已有账号？<router-link to="/login">前往登录</router-link>
      </p>
    </form>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()
const auth = useAuthStore()
const email = ref('')
const username = ref('')
const password = ref('')
const error = ref('')
const loading = ref(false)

const canSubmit = computed(
  () => email.value.length > 0 && username.value.length >= 3 && password.value.length >= 8
)

async function submit(): Promise<void> {
  if (!canSubmit.value) return
  error.value = ''
  loading.value = true
  try {
    await auth.register({ email: email.value, username: username.value, password: password.value })
    await router.push('/')
  } catch (e) {
    error.value = (e as Error).message
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.auth-page {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--bg);
}
.auth-card {
  background: var(--bg-elev);
  border: 1px solid var(--border);
  padding: 24px;
  border-radius: 8px;
  width: 360px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}
.auth-error { color: var(--danger); font-size: 14px; margin: 4px 0; }
.auth-link { font-size: 14px; color: var(--text-muted); text-align: center; }
</style>
