<template>
  <div class="view-selector">
    <div class="view-tabs">
      <button
        v-for="view in views"
        :key="view.id"
        :class="['view-tab', { 'view-tab--active': currentView?.id === view.id }]"
        @click="selectView(view)"
      >
        <span class="view-tab-icon">{{ viewIcon(view.type) }}</span>
        <span class="view-tab-name">{{ view.name }}</span>
      </button>
      <button class="view-tab view-tab--settings" @click="$router.push('/views')" title="视图管理">
        ⚙️
      </button>
    </div>
    <div v-if="currentView" class="view-info">
      <span class="view-type-badge">{{ viewTypeLabel(currentView.type) }}</span>
      <button class="btn-text btn-sm" @click="$router.push('/views')">配置</button>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, watch, inject } from 'vue'
import { useRouter } from 'vue-router'
import api from '../api.js'

const emit = defineEmits(['view-changed'])
const router = useRouter()
const showToast = inject('showToast')
const views = ref([])
const currentView = ref(null)

// 加载视图列表
async function loadViews() {
  const res = await api.listViews()
  if (res.code === 0) {
    views.value = res.data
    // 选择默认视图
    const def = res.data.find(v => v.is_default) || res.data[0]
    if (def) selectView(def)
  }
}

// 选择视图
function selectView(view) {
  currentView.value = view
  localStorage.setItem('currentViewId', String(view.id))
  emit('view-changed', view)
}

function viewIcon(type) {
  const icons = { table: '📋', board: '📊', calendar: '📅', gantt: '📈' }
  return icons[type] || '📋'
}

function viewTypeLabel(type) {
  const labels = { table: '表格', board: '看板', calendar: '日历', gantt: '甘特图' }
  return labels[type] || '表格'
}

// 恢复上次选择的视图
onMounted(async () => {
  await loadViews()
  const savedId = localStorage.getItem('currentViewId')
  if (savedId && views.value.length > 0) {
    const saved = views.value.find(v => String(v.id) === savedId)
    if (saved) selectView(saved)
  }
})

defineExpose({ currentView, loadViews })
</script>

<style scoped>
.view-selector {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 16px;
  background: var(--card-bg, #fff);
  border-bottom: 1px solid var(--border-color, #e0e0e0);
}
.view-tabs {
  display: flex;
  gap: 4px;
  overflow-x: auto;
}
.view-tab {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 12px;
  border: none;
  background: transparent;
  border-radius: 6px;
  cursor: pointer;
  font-size: 14px;
  white-space: nowrap;
  transition: all 0.2s;
}
.view-tab:hover {
  background: var(--hover-bg, #f5f5f5);
}
.view-tab--active {
  background: var(--primary-color, #4A90D9);
  color: white;
}
.view-tab-icon {
  font-size: 16px;
}
.view-tab-name {
  font-weight: 500;
}
.view-tab--settings {
  padding: 6px 8px;
}
.view-info {
  display: flex;
  align-items: center;
  gap: 8px;
}
.view-type-badge {
  font-size: 12px;
  padding: 2px 8px;
  background: var(--badge-bg, #f0f0f0);
  border-radius: 4px;
  color: var(--text-secondary, #666);
}
</style>
