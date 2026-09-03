<template>
  <div id="app">
    <nav class="nav">
      <router-link to="/" class="nav-link">📋 列表</router-link>
      <router-link to="/board" class="nav-link">📊 看板</router-link>
      <router-link to="/stats" class="nav-link">📈 统计</router-link>
      <router-link to="/groups" class="nav-link">🏷️ 分组</router-link>
    </nav>
    <router-view />
    <div v-if="toast" :class="['toast', 'toast-' + toast.type]">{{ toast.msg }}</div>
  </div>
</template>

<script setup>
import { ref, provide, onMounted, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'

const toast = ref(null)
const router = useRouter()

function showToast(msg, type = 'success') {
  toast.value = { msg, type }
  setTimeout(() => { toast.value = null }, 2200)
}

provide('showToast', showToast)

function handleKeydown(e) {
  // 忽略输入框内的按键
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
    if (e.key === 'Escape') e.target.blur()
    return
  }

  switch (e.key.toLowerCase()) {
    case 'n':
      // 触发新建 - 通过路由向子组件传递
      const createBtn = document.querySelector('.btn-primary.btn-sm')
      if (createBtn && createBtn.textContent.includes('新建')) createBtn.click()
      break
    case '/':
      e.preventDefault()
      const searchInput = document.querySelector('.search-input, .form-input[placeholder*="搜索"]')
      if (searchInput) searchInput.focus()
      break
    case 'Escape':
      // 关闭弹窗由子组件自行处理
      const closeBtn = document.querySelector('.close-btn')
      if (closeBtn) closeBtn.click()
      break
    case '1': router.push('/'); break
    case '2': router.push('/board'); break
    case '3': router.push('/stats'); break
    case '4': router.push('/groups'); break
  }
}

onMounted(() => { document.addEventListener('keydown', handleKeydown) })
onUnmounted(() => { document.removeEventListener('keydown', handleKeydown) })
</script>
