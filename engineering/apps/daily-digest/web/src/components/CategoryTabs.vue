<template>
  <div class="category-tabs" role="tablist">
    <button
      v-for="tab in tabs"
      :key="tab.key"
      class="tab"
      :class="{ active: activeCategory === tab.key }"
      @click="$emit('change', tab.key)"
      role="tab"
      :aria-selected="activeCategory === tab.key"
    >
      {{ tab.label }}
      <sup v-if="tab.count !== undefined" class="tab-count">{{ tab.count }}</sup>
    </button>
  </div>
</template>

<script setup lang="ts">
interface Tab {
  key: string
  label: string
  count?: number
}

defineProps<{
  tabs: Tab[]
  activeCategory: string
}>()

defineEmits<{
  change: [key: string]
}>()
</script>

<style scoped>
.category-tabs {
  display: flex;
  gap: var(--space-1, 4px);
  padding: var(--space-3, 12px) 0;
  overflow-x: auto;
  -webkit-overflow-scrolling: touch;
  scrollbar-width: none;
}

.category-tabs::-webkit-scrollbar {
  display: none;
}

.tab {
  flex-shrink: 0;
  padding: var(--space-1, 4px) var(--space-4, 16px);
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-sm, 0.8125rem);
  font-weight: var(--weight-medium, 500);
  color: var(--color-text-secondary, #5A6178);
  background: var(--color-surface, #fff);
  border: 1px solid var(--color-border, #E2E5EC);
  border-radius: var(--radius-full, 9999px);
  cursor: pointer;
  transition: all var(--transition-fast, 150ms);
  white-space: nowrap;
}

.tab:hover {
  color: var(--color-text-primary, #1A1A2E);
  border-color: var(--color-text-tertiary, #9CA3AF);
}

.tab.active {
  color: #fff;
  background: var(--color-brand, #2D7FF9);
  border-color: var(--color-brand, #2D7FF9);
}

.tab-count {
  font-size: 0.65rem;
  margin-left: 3px;
  opacity: 0.8;
}
</style>
