<template>
  <div class="history-view">
    <h1 class="page-title">阅读历史</h1>

    <div class="item-list" v-if="items.length">
      <ItemCard v-for="item in items" :key="item.id" :item="item" />
    </div>

    <div class="empty-state" v-else>
      <p>暂无阅读历史</p>
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
import { ref, onMounted, watch } from 'vue'
import ItemCard from '../components/ItemCard.vue'
import Pagination from '../components/Pagination.vue'
import { api, type Item } from '../api/client'

const items = ref<Item[]>([])
const total = ref(0)
const page = ref(1)
const pageSize = ref(10) // 响应式每页条数，默认 10

async function loadHistory() {
  try {
    const resp = await api.getDaily(undefined, page.value, pageSize.value) as any
    items.value = resp.items
    total.value = resp.total
  } catch (e) {
    console.error('加载历史失败:', e)
  }
}

watch(page, loadHistory)
onMounted(loadHistory)

// 监听每页条数变化：重置到第 1 页并重新加载
watch(pageSize, () => {
  page.value = 1
  loadHistory()
})
</script>

<style scoped>
.page-title {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-2xl, 1.5rem);
  font-weight: var(--weight-bold, 700);
  margin: 0 0 var(--space-6, 24px);
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
</style>