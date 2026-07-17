<template>
  <div class="subscriptions-view">
    <h1 class="page-title">订阅管理</h1>

    <div class="sub-list" v-if="subscriptions.length">
      <div
        class="sub-card"
        v-for="sub in subscriptions"
        :key="sub.id"
      >
        <div class="sub-info">
          <span class="sub-category">{{ categoryLabel(sub.category) }}</span>
          <span class="sub-keywords" v-if="sub.keywords !== '[]'">
            {{ sub.keywords }}
          </span>
        </div>
        <button class="btn btn-ghost btn-sm" @click="handleDelete(sub.id)">
          取消订阅
        </button>
      </div>
    </div>

    <div class="empty-state" v-else>
      <p>暂无订阅，选择方向开始订阅</p>
    </div>

    <div class="add-section">
      <h3>添加订阅</h3>
      <div class="add-form">
        <select v-model="newCategory" class="input">
          <option value="db">数据库</option>
          <option value="ai">AI</option>
          <option value="ml">ML</option>
          <option value="infra">基础设施</option>
        </select>
        <button class="btn btn-primary" @click="handleAdd">订阅</button>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api, type Subscription } from '../api/client'

const subscriptions = ref<Subscription[]>([])
const newCategory = ref('db')

const CATEGORY_LABELS: Record<string, string> = {
  db: '数据库',
  ai: '人工智能',
  ml: '机器学习',
  infra: '基础设施',
  sys: '系统',
}

function categoryLabel(key: string): string {
  return CATEGORY_LABELS[key] || key
}

async function loadSubscriptions() {
  try {
    subscriptions.value = await api.getCollections(1) as any
    // 使用 api 中的每日接口获取订阅 — 简化版直接用 fetch
    const resp = await fetch('/api/v1/subscriptions?user_id=1')
    if (resp.ok) {
      subscriptions.value = await resp.json()
    }
  } catch (e) {
    console.error('加载订阅失败:', e)
  }
}

async function handleAdd() {
  try {
    await fetch('/api/v1/subscriptions', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ user_id: 1, category: newCategory.value }),
    })
    await loadSubscriptions()
  } catch (e) {
    console.error('添加订阅失败:', e)
  }
}

async function handleDelete(id: number) {
  try {
    await fetch(`/api/v1/subscriptions/${id}`, { method: 'DELETE' })
    await loadSubscriptions()
  } catch (e) {
    console.error('取消订阅失败:', e)
  }
}

onMounted(loadSubscriptions)
</script>

<style scoped>
.page-title {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-2xl, 1.5rem);
  font-weight: var(--weight-bold, 700);
  margin: 0 0 var(--space-6, 24px);
}

.sub-list {
  display: flex;
  flex-direction: column;
  gap: var(--space-3, 12px);
  margin-bottom: var(--space-8, 32px);
}

.sub-card {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: var(--color-surface, #fff);
  border-radius: var(--radius-lg, 12px);
  padding: var(--space-4, 16px) var(--space-5, 20px);
  box-shadow: var(--shadow-card, 0 1px 3px rgba(0,0,0,0.06));
}

.sub-category {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-weight: var(--weight-semibold, 600);
  font-size: var(--text-base, 0.9375rem);
}

.sub-keywords {
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-tertiary, #9CA3AF);
  margin-left: var(--space-3, 12px);
}

.empty-state {
  text-align: center;
  padding: var(--space-12, 48px);
  color: var(--color-text-secondary, #5A6178);
}

.add-section {
  background: var(--color-surface, #fff);
  border-radius: var(--radius-lg, 12px);
  padding: var(--space-5, 20px);
  box-shadow: var(--shadow-card, 0 1px 3px rgba(0,0,0,0.06));
}

.add-section h3 {
  margin: 0 0 var(--space-3, 12px);
  font-size: var(--text-base, 0.9375rem);
}

.add-form {
  display: flex;
  gap: var(--space-3, 12px);
}

.add-form select {
  flex: 1;
}
</style>