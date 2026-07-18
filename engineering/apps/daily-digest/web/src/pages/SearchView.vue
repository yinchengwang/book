<template>
  <div class="search-view">
    <h1 class="page-title">搜索</h1>

    <div class="search-bar">
      <input
        v-model="query"
        type="text"
        class="search-input"
        placeholder="搜索技术内容..."
        @keyup.enter="doSearch"
      />
      <button class="btn btn-primary" @click="doSearch">搜索</button>
    </div>

    <CategoryTabs
      :tabs="categoryTabs"
      :active-category="activeCategory"
      @change="onCategoryChange"
    />

    <div class="item-list" v-if="items.length">
      <ItemCard v-for="item in items" :key="item.id" :item="item" />
    </div>

    <div class="empty-state" v-else-if="searched && !loading">
      <p>未找到相关内容</p>
    </div>

    <div class="loading-state" v-if="loading">
      <span class="loader"></span>
      <span>搜索中...</span>
    </div>

    <Pagination
      v-model="page"
      v-model:size="pageSize"
      :total="total"
      :size="pageSize"
    />
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from 'vue'
import CategoryTabs from '../components/CategoryTabs.vue'
import ItemCard from '../components/ItemCard.vue'
import Pagination from '../components/Pagination.vue'
import { api, type Item } from '../api/client'

const query = ref('')
const items = ref<Item[]>([])
const total = ref(0)
const page = ref(1)
const pageSize = ref(10) // 响应式每页条数，默认 10
const loading = ref(false)
const searched = ref(false)
const activeCategory = ref('')

const categoryTabs = [
  { key: '', label: '全部' },
  { key: 'db', label: '数据库' },
  { key: 'ai', label: 'AI' },
  { key: 'ml', label: 'ML' },
  { key: 'infra', label: '基础设施' },
]

async function doSearch() {
  if (!query.value.trim()) return
  loading.value = true
  searched.value = true
  try {
    const resp = await api.search(
      query.value,
      activeCategory.value || undefined,
      page.value,
      pageSize.value // 使用 ref 的 .value
    )
    items.value = resp.items
    total.value = resp.total
  } catch (e) {
    console.error('搜索失败:', e)
    items.value = []
    total.value = 0
  } finally {
    loading.value = false
  }
}

function onCategoryChange(key: string) {
  activeCategory.value = key
  page.value = 1
  if (query.value.trim()) {
    doSearch()
  }
}

watch(page, () => {
  if (query.value.trim()) {
    doSearch()
  }
})

// 监听每页条数变化：重置到第 1 页并重新搜索
watch(pageSize, () => {
  page.value = 1
  if (query.value.trim()) {
    doSearch()
  }
})
</script>

<style scoped>
.search-view {
  padding-bottom: var(--space-8, 32px);
}

.page-title {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-2xl, 1.5rem);
  font-weight: var(--weight-bold, 700);
  margin: 0 0 var(--space-4, 16px);
}

.search-bar {
  display: flex;
  gap: var(--space-3, 12px);
  margin-bottom: var(--space-2, 8px);
}

.search-input {
  flex: 1;
  padding: var(--space-3, 12px) var(--space-4, 16px);
  font-family: var(--font-body, sans-serif);
  font-size: var(--text-base, 0.9375rem);
  color: var(--color-text-primary, #1A1A2E);
  background: var(--color-surface, #fff);
  border: 1px solid var(--color-border, #E2E5EC);
  border-radius: var(--radius-md, 8px);
  outline: none;
  transition: border-color var(--transition-fast, 150ms);
}

.search-input:focus {
  border-color: var(--color-brand, #2D7FF9);
  box-shadow: 0 0 0 3px rgba(45, 127, 249, 0.15);
}

.item-list {
  display: flex;
  flex-direction: column;
  gap: var(--space-5, 20px);
  border-left: 2px dashed var(--color-border, #E2E5EC);
  padding-left: var(--space-4, 16px);
  margin-left: var(--space-2, 8px);
}

.empty-state {
  text-align: center;
  padding: var(--space-12, 48px);
  color: var(--color-text-secondary, #5A6178);
}

.loading-state {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: var(--space-3, 12px);
  padding: var(--space-12, 48px);
  color: var(--color-text-tertiary, #9CA3AF);
}

.loader {
  width: 20px;
  height: 20px;
  border: 2px solid var(--color-border, #E2E5EC);
  border-top-color: var(--color-brand, #2D7FF9);
  border-radius: 50%;
  animation: spin 0.6s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}
</style>