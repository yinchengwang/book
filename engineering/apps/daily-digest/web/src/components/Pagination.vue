<template>
  <div class="pagination" v-if="totalPages > 1">
    <button
      class="page-btn"
      :disabled="modelValue <= 1"
      @click="go(modelValue - 1)"
    >
      ← 上一页
    </button>

    <span class="page-info">
      {{ modelValue }} / {{ totalPages }}
    </span>

    <button
      class="page-btn"
      :disabled="modelValue >= totalPages"
      @click="go(modelValue + 1)"
    >
      下一页 →
    </button>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = defineProps<{
  modelValue: number
  total: number
  size: number
}>()

const emit = defineEmits<{
  'update:modelValue': [page: number]
}>()

const totalPages = computed(() => Math.ceil(props.total / props.size) || 1)

function go(page: number) {
  if (page >= 1 && page <= totalPages.value) {
    emit('update:modelValue', page)
  }
}
</script>

<style scoped>
.pagination {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: var(--space-4, 16px);
  padding: var(--space-8, 32px) 0;
}

.page-btn {
  padding: var(--space-2, 8px) var(--space-4, 16px);
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-sm, 0.8125rem);
  font-weight: var(--weight-medium, 500);
  color: var(--color-text-primary, #1A1A2E);
  background: var(--color-surface, #fff);
  border: 1px solid var(--color-border, #E2E5EC);
  border-radius: var(--radius-md, 8px);
  cursor: pointer;
  transition: all var(--transition-fast, 150ms);
}

.page-btn:hover:not(:disabled) {
  border-color: var(--color-brand, #2D7FF9);
  color: var(--color-brand, #2D7FF9);
}

.page-btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.page-info {
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-tertiary, #9CA3AF);
}
</style>
