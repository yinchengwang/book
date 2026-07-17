<template>
  <div class="collections-view">
    <h1 class="page-title">收藏夹</h1>

    <div class="collection-list" v-if="collections.length">
      <div
        class="collection-card"
        v-for="col in collections"
        :key="col.id"
      >
        <div class="collection-info">
          <h3 class="collection-name">{{ col.name }}</h3>
          <p class="collection-desc" v-if="col.description">{{ col.description }}</p>
          <span class="collection-count">{{ col.item_count }} 项</span>
        </div>
        <button
          class="btn btn-ghost btn-sm"
          @click="handleDelete(col.id)"
        >
          删除
        </button>
      </div>
    </div>

    <div class="empty-state" v-else>
      <p>暂无收藏夹</p>
    </div>

    <div class="create-section">
      <input
        v-model="newName"
        type="text"
        class="input"
        placeholder="新建收藏夹名称"
        @keyup.enter="handleCreate"
      />
      <button class="btn btn-primary" @click="handleCreate">新建</button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api, type Collection } from '../api/client'

const collections = ref<Collection[]>([])
const newName = ref('')

async function loadCollections() {
  try {
    collections.value = await api.getCollections(1)
  } catch (e) {
    console.error('加载收藏夹失败:', e)
  }
}

async function handleCreate() {
  if (!newName.value.trim()) return
  try {
    await api.createCollection(1, newName.value)
    newName.value = ''
    await loadCollections()
  } catch (e) {
    console.error('创建收藏夹失败:', e)
  }
}

async function handleDelete(id: number) {
  try {
    await api.deleteCollection(id)
    await loadCollections()
  } catch (e) {
    console.error('删除收藏夹失败:', e)
  }
}

onMounted(loadCollections)
</script>

<style scoped>
.page-title {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-2xl, 1.5rem);
  font-weight: var(--weight-bold, 700);
  margin: 0 0 var(--space-6, 24px);
}

.collection-list {
  display: flex;
  flex-direction: column;
  gap: var(--space-3, 12px);
  margin-bottom: var(--space-6, 24px);
}

.collection-card {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: var(--color-surface, #fff);
  border-radius: var(--radius-lg, 12px);
  padding: var(--space-4, 16px) var(--space-5, 20px);
  box-shadow: var(--shadow-card, 0 1px 3px rgba(0,0,0,0.06));
}

.collection-name {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-base, 0.9375rem);
  font-weight: var(--weight-semibold, 600);
  margin: 0 0 4px;
}

.collection-desc {
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-secondary, #5A6178);
  margin: 0 0 4px;
}

.collection-count {
  font-size: var(--text-xs, 0.75rem);
  color: var(--color-text-tertiary, #9CA3AF);
}

.empty-state {
  text-align: center;
  padding: var(--space-12, 48px);
  color: var(--color-text-secondary, #5A6178);
}

.create-section {
  display: flex;
  gap: var(--space-3, 12px);
}

.create-section .input {
  flex: 1;
}
</style>