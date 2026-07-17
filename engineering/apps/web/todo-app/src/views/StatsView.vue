<template>
  <div class="stats-view">
    <h2>📊 统计看板</h2>
    <div class="stats-grid">
      <div class="stat-card">
        <div class="stat-icon">📝</div>
        <div class="stat-info">
          <span class="stat-value">{{ stats.total }}</span>
          <span class="stat-label">总待办</span>
        </div>
      </div>
      <div class="stat-card">
        <div class="stat-icon">✅</div>
        <div class="stat-info">
          <span class="stat-value">{{ stats.done }}</span>
          <span class="stat-label">已完成</span>
        </div>
      </div>
      <div class="stat-card">
        <div class="stat-icon">⏳</div>
        <div class="stat-info">
          <span class="stat-value">{{ stats.open }}</span>
          <span class="stat-label">进行中</span>
        </div>
      </div>
      <div class="stat-card" :class="{ alert: stats.overdue > 0 }">
        <div class="stat-icon">⚠️</div>
        <div class="stat-info">
          <span class="stat-value">{{ stats.overdue }}</span>
          <span class="stat-label">已逾期</span>
        </div>
      </div>
    </div>
    <div class="stats-breakdown">
      <div class="breakdown-section">
        <h3>按优先级</h3>
        <div v-for="p in priorityStats" :key="p.level" class="breakdown-row">
          <span class="breakdown-label">{{ p.name }}</span>
          <div class="breakdown-bar-bg">
            <div class="breakdown-bar-fill" :style="{ width: p.pct + '%' }"></div>
          </div>
          <span class="breakdown-count">{{ p.count }}</span>
        </div>
      </div>
      <div class="breakdown-section">
        <h3>按分组</h3>
        <div v-for="g in groupStats" :key="g.id" class="breakdown-row">
          <span class="breakdown-label">{{ g.name }}</span>
          <div class="breakdown-bar-bg">
            <div class="breakdown-bar-fill" :style="{ width: g.pct + '%' }"></div>
          </div>
          <span class="breakdown-count">{{ g.count }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import api from '../api.js'

const todos = ref([])
const groups = ref([])
const now = Date.now() / 1000

const stats = computed(() => {
  const total = todos.value.length
  const done = todos.value.filter(t => t.status === 'done').length
  const open = todos.value.filter(t => t.status === 'open').length
  const overdue = todos.value.filter(t => t.status !== 'done' && t.due_date > 0 && t.due_date < now).length
  return { total, done, open, overdue }
})

const PRIORITY_NAMES = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']
const priorityStats = computed(() => {
  const total = todos.value.length || 1
  return PRIORITY_NAMES.map((name, i) => {
    const count = todos.value.filter(t => t.priority === i).length
    return { level: i, name, count, pct: Math.round(count / total * 100) }
  })
})

const groupStats = computed(() => {
  const total = todos.value.length || 1
  const rows = [{ id: 0, name: '未分组', count: todos.value.filter(t => t.group_id === 0).length }]
  groups.value.forEach(g => {
    rows.push({ id: g.id, name: g.name, count: todos.value.filter(t => t.group_id === g.id).length })
  })
  return rows.map(r => ({ ...r, pct: Math.round(r.count / total * 100) }))
})

async function loadData() {
  const r = await api.list({ status: 'all', per_page: 1000 })
  if (r.code === 0) todos.value = r.data.items
  const rg = await api.listGroups()
  if (rg.code === 0) groups.value = rg.data
}

onMounted(loadData)
</script>

<style scoped>
.stats-view { padding: 24px; max-width: 900px; margin: 0 auto; }
.stats-view h2 { margin-bottom: 24px; }
.stats-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 16px; margin-bottom: 32px; }
.stat-card { background: var(--bg-elev); padding: 20px; border-radius: var(--radius); display: flex; align-items: center; gap: 16px; }
.stat-card.alert { border: 1px solid var(--danger); }
.stat-icon { font-size: 32px; }
.stat-info { display: flex; flex-direction: column; }
.stat-value { font-size: 28px; font-weight: 700; }
.stat-label { color: var(--text-muted); font-size: 14px; }
.stats-breakdown { display: grid; grid-template-columns: 1fr 1fr; gap: 32px; }
.breakdown-section h3 { margin-bottom: 16px; font-size: 16px; }
.breakdown-row { display: flex; align-items: center; gap: 12px; margin-bottom: 12px; }
.breakdown-label { width: 80px; font-size: 14px; }
.breakdown-bar-bg { flex: 1; height: 8px; background: var(--bg-base); border-radius: 4px; }
.breakdown-bar-fill { height: 100%; background: var(--primary); border-radius: 4px; }
.breakdown-count { width: 40px; text-align: right; font-size: 14px; color: var(--text-muted); }
</style>
