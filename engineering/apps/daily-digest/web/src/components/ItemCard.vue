<template>
  <article
    class="item-card"
    :class="[`source-line-${item.source}`]"
    @click="$router.push(`/item/${item.id}`)"
  >
    <div class="card-header">
      <span class="card-source" :class="`source-color-${item.source}`">
        {{ sourceLabel }}
      </span>
      <span class="card-score" v-if="item.score > 0">
        {{ (item.score * 100).toFixed(0) }}
      </span>
    </div>

    <h3 class="card-title">{{ item.title }}</h3>

    <p class="card-summary" v-if="item.summary">
      {{ truncate(item.summary, 120) }}
    </p>

    <div class="card-footer">
      <div class="card-tags" v-if="item.tags && item.tags.length">
        <span class="tag" v-for="tag in item.tags.slice(0, 4)" :key="tag">
          {{ tag }}
        </span>
      </div>
      <time class="card-time" v-if="item.published">
        {{ formatTime(item.published) }}
      </time>
    </div>
  </article>
</template>

<script setup lang="ts">
import type { Item } from '../api/client'

const props = defineProps<{
  item: Item
}>()

const SOURCE_LABELS: Record<string, string> = {
  arxiv: 'arXiv',
  semantic_scholar: 'Semantic Scholar',
  dblp: 'DBLP',
  huggingface: 'HuggingFace',
  blog: '博客',
  github: 'GitHub',
}

const sourceLabel = SOURCE_LABELS[props.item.source] || props.item.source

function truncate(text: string, len: number): string {
  return text.length > len ? text.slice(0, len) + '...' : text
}

function formatTime(dateStr: string): string {
  const d = new Date(dateStr)
  const now = new Date()
  const diff = now.getTime() - d.getTime()
  const days = Math.floor(diff / 86400000)
  if (days === 0) return '今天'
  if (days === 1) return '昨天'
  if (days < 7) return `${days} 天前`
  return `${d.getMonth() + 1}/${d.getDate()}`
}
</script>

<style scoped>
.item-card {
  background: var(--color-surface, #fff);
  border-radius: var(--radius-lg, 12px);
  padding: var(--space-5, 20px);
  cursor: pointer;
  transition: all var(--transition-fast, 150ms);
  border-left: 4px solid transparent;
  box-shadow: var(--shadow-card, 0 1px 3px rgba(0,0,0,0.06));
}

.item-card:hover {
  box-shadow: var(--shadow-card-hover, 0 4px 12px rgba(0,0,0,0.08));
  transform: translateY(-1px);
}

.card-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: var(--space-2, 8px);
}

.card-source {
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-xs, 0.75rem);
  font-weight: var(--weight-medium, 500);
  padding: 2px 8px;
  border-radius: var(--radius-full, 9999px);
  background: color-mix(in srgb, var(--color-brand, #2D7FF9) 10%, transparent);
  color: var(--color-brand, #2D7FF9);
}

.card-score {
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-xs, 0.75rem);
  color: var(--color-text-tertiary, #9CA3AF);
}

.card-title {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-lg, 1.0625rem);
  font-weight: var(--weight-semibold, 600);
  color: var(--color-text-primary, #1A1A2E);
  line-height: 1.4;
  margin: 0 0 var(--space-2, 8px);
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  overflow: hidden;
}

.card-summary {
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-secondary, #5A6178);
  line-height: 1.6;
  margin: 0 0 var(--space-3, 12px);
}

.card-footer {
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: var(--space-2, 8px);
}

.card-tags {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
}

.tag {
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: 0.7rem;
  padding: 1px 6px;
  border-radius: var(--radius-sm, 4px);
  background: var(--color-surface-hover, #F3F4F6);
  color: var(--color-text-tertiary, #9CA3AF);
}

.card-time {
  font-size: var(--text-xs, 0.75rem);
  color: var(--color-text-tertiary, #9CA3AF);
  white-space: nowrap;
}
</style>
