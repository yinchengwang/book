<template>
  <div class="daily-view">
    <!-- 数据波纹可视化 -->
    <DataRipple
      :categories="categoryTabs"
      :active-category="activeCategory"
      @category-change="onCategoryChange"
    />

    <!-- 分类筛选标签 -->
    <CategoryTabs
      :tabs="categoryTabs"
      :active-category="activeCategory"
      @change="onCategoryChange"
    />

    <!-- 内容列表 -->
    <div class="item-list" v-if="items.length">
      <ItemCard v-for="item in items" :key="item.id" :item="item" />
    </div>

    <!-- 空状态 -->
    <div class="empty-state" v-else-if="!loading">
      <p>今日暂无更新</p>
      <p class="empty-hint">数据聚合将在每天 08:00 自动执行</p>
    </div>

    <!-- 加载状态 -->
    <div class="loading-state" v-if="loading">
      <span class="loader"></span>
      <span>加载中...</span>
    </div>

    // 分页
    <Pagination
      v-model="page"
      v-model:size="pageSize"
      :total="total"
      :size="pageSize"
    />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, watch } from 'vue'
import DataRipple from '../components/DataRipple.vue'
import CategoryTabs from '../components/CategoryTabs.vue'
import ItemCard from '../components/ItemCard.vue'
import Pagination from '../components/Pagination.vue'
import { api, type Item } from '../api/client'

const CATEGORY_CONFIG = [
  { key: '', label: '全部' },
  { key: 'db', label: '数据库' },
  { key: 'ai', label: 'AI' },
  { key: 'ml', label: 'ML' },
  { key: 'infra', label: '基础设施' },
  { key: 'sys', label: '系统' },
]

const items = ref<Item[]>([])
const total = ref(0)
const page = ref(1)
const pageSize = ref(10) // 响应式每页条数，默认 10
const loading = ref(false)
const activeCategory = ref('')

// 含计数的分类标签（用于 DataRipple）
const categoryTabs = ref(CATEGORY_CONFIG.map(c => ({ key: c.key, label: c.label })))

async function loadItems() {
  loading.value = true
  try {
    const resp = await api.getDaily(
      activeCategory.value || undefined,
      page.value,
      pageSize.value // 使用 ref 的 .value
    )
    items.value = resp.items
    total.value = resp.total
  } catch (e) {
    console.error('加载失败:', e)
    items.value = []
    total.value = 0
  } finally {
    loading.value = false
  }
}

function onCategoryChange(key: string) {
  activeCategory.value = key
  page.value = 1
}

// 监听分类和页码变化
watch([activeCategory, page], () => {
  loadItems()
})

// 监听每页条数变化：重置到第 1 页并重新加载
watch(pageSize, () => {
  page.value = 1
  loadItems()
})

onMounted(() => {
  loadItems()
})
</script>

<style scoped>
.daily-view {
  padding-bottom: var(--space-8, 32px);
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
  padding: var(--space-12, 48px) var(--space-4, 16px);
  color: var(--color-text-secondary, #5A6178);
}

.empty-hint {
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-tertiary, #9CA3AF);
  margin-top: var(--space-2, 8px);
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