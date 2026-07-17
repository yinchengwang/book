<template>
  <div class="detail-view" v-if="item">
    <button class="back-btn" @click="$router.back()">← 返回</button>

    <article class="detail-card">
      <div class="detail-header">
        <span class="detail-source" :class="`source-color-${item.source}`">
          {{ sourceLabel }}
        </span>
        <span class="detail-score" v-if="item.score > 0">
          评分 {{ (item.score * 100).toFixed(0) }}
        </span>
      </div>

      <h1 class="detail-title">{{ item.title }}</h1>

      <div class="detail-meta">
        <time v-if="item.published">
          {{ new Date(item.published).toLocaleDateString('zh-CN') }}
        </time>
        <span class="detail-category" v-if="item.category">
          {{ categoryLabel }}
        </span>
      </div>

      <div class="detail-summary" v-if="item.summary">
        <h3>摘要</h3>
        <p>{{ item.summary }}</p>
      </div>

      <div class="detail-raw" v-if="item.raw_content">
        <h3>原文内容</h3>
        <p>{{ item.raw_content }}</p>
      </div>

      <div class="detail-tags" v-if="item.tags?.length">
        <span class="tag" v-for="tag in item.tags" :key="tag">{{ tag }}</span>
      </div>

      <div class="detail-actions">
        <a :href="item.url" target="_blank" rel="noopener" class="btn btn-primary">
          查看原文 ↗
        </a>
      </div>
    </article>
  </div>

  <div class="loading-state" v-else>
    <span class="loader"></span>
    <span>加载中...</span>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { api, type Item } from '../api/client'

const route = useRoute()
const item = ref<Item | null>(null)

const SOURCE_LABELS: Record<string, string> = {
  arxiv: 'arXiv',
  semantic_scholar: 'Semantic Scholar',
  dblp: 'DBLP',
  huggingface: 'HuggingFace',
  blog: '博客',
  github: 'GitHub',
}

const CATEGORY_LABELS: Record<string, string> = {
  db: '数据库',
  ai: '人工智能',
  ml: '机器学习',
  infra: '基础设施',
  sys: '系统',
  other: '其他',
}

const sourceLabel = SOURCE_LABELS[item.value?.source || ''] || item.value?.source || ''
const categoryLabel = CATEGORY_LABELS[item.value?.category || ''] || item.value?.category || ''

onMounted(async () => {
  try {
    const id = Number(route.params.id)
    item.value = await api.getItem(id)
  } catch (e) {
    console.error('加载详情失败:', e)
  }
})
</script>

<style scoped>
.detail-view {
  max-width: 720px;
  margin: 0 auto;
}

.back-btn {
  background: none;
  border: none;
  color: var(--color-brand, #2D7FF9);
  font-size: var(--text-sm, 0.8125rem);
  cursor: pointer;
  padding: var(--space-2, 8px) 0;
  margin-bottom: var(--space-4, 16px);
}

.detail-card {
  background: var(--color-surface, #fff);
  border-radius: var(--radius-lg, 12px);
  padding: var(--space-8, 32px);
  box-shadow: var(--shadow-card, 0 1px 3px rgba(0,0,0,0.06));
}

.detail-header {
  display: flex;
  align-items: center;
  gap: var(--space-3, 12px);
  margin-bottom: var(--space-4, 16px);
}

.detail-source {
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-xs, 0.75rem);
  padding: 2px 10px;
  border-radius: var(--radius-full, 9999px);
  background: color-mix(in srgb, var(--color-brand, #2D7FF9) 10%, transparent);
  color: var(--color-brand, #2D7FF9);
}

.detail-score {
  font-size: var(--text-xs, 0.75rem);
  color: var(--color-text-tertiary, #9CA3AF);
}

.detail-title {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-2xl, 1.5rem);
  font-weight: var(--weight-bold, 700);
  line-height: 1.3;
  margin: 0 0 var(--space-3, 12px);
  color: var(--color-text-primary, #1A1A2E);
}

.detail-meta {
  display: flex;
  gap: var(--space-3, 12px);
  margin-bottom: var(--space-6, 24px);
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-tertiary, #9CA3AF);
}

.detail-summary,
.detail-raw {
  margin-bottom: var(--space-6, 24px);
}

.detail-summary h3,
.detail-raw h3 {
  font-size: var(--text-base, 0.9375rem);
  font-weight: var(--weight-semibold, 600);
  margin: 0 0 var(--space-2, 8px);
  color: var(--color-text-primary, #1A1A2E);
}

.detail-summary p,
.detail-raw p {
  font-size: var(--text-base, 0.9375rem);
  line-height: 1.7;
  color: var(--color-text-secondary, #5A6178);
  white-space: pre-wrap;
}

.detail-tags {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
  margin-bottom: var(--space-6, 24px);
}

.tag {
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: 0.7rem;
  padding: 2px 8px;
  border-radius: var(--radius-sm, 4px);
  background: var(--color-surface-hover, #F3F4F6);
  color: var(--color-text-tertiary, #9CA3AF);
}

.detail-actions {
  display: flex;
  gap: var(--space-3, 12px);
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