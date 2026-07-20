<template>
  <div id="app">
    <nav class="nav">
      <router-link to="/" class="nav-link">📋 列表</router-link>
      <router-link to="/board" class="nav-link">📊 看板</router-link>
      <router-link to="/stats" class="nav-link">📈 统计</router-link>
      <router-link to="/groups" class="nav-link">🏷️ 分组</router-link>
      <router-link to="/calendar" class="nav-link">📅 日历</router-link>
      <router-link to="/stats-dfx" class="nav-link">📊 DFX</router-link>
      <router-link to="/plan-manage" class="nav-link">📋 计划</router-link>
      <router-link to="/views" class="nav-link">🎨 视图</router-link>
    </nav>
    <router-view />
    <CarryoverModal :show="showCarryover" :tasks="carryoverTasks" @close="showCarryover = false" @confirm="handleCarryoverConfirm" />
    <div v-if="toast" :class="['toast', 'toast-' + toast.type]">{{ toast.msg }}</div>
  </div>
</template>

<script setup>
import { ref, provide, onMounted, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import CarryoverModal from './components/CarryoverModal.vue'
import api from './api.js'

const toast = ref(null)
const router = useRouter()

const showCarryover = ref(false)
const carryoverTasks = ref([])

async function checkCarryover() {
  const res = await api.pendingCarryover()
  if (res.code === 0 && res.data && res.data.items && res.data.items.length > 0) {
    carryoverTasks.value = res.data.items
    showCarryover.value = true
  }
}

async function handleCarryoverConfirm(items) {
  await api.confirmCarryover(items)
  showCarryover.value = false
  showToast('过期任务处理完成')
}

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
    case '5': router.push('/calendar'); break
    case '6': router.push('/stats-dfx'); break
    case '7': router.push('/plan-manage'); break
  }
}

onMounted(() => {
  document.addEventListener('keydown', handleKeydown)
  checkCarryover()
})
onUnmounted(() => { document.removeEventListener('keydown', handleKeydown) })
</script>
