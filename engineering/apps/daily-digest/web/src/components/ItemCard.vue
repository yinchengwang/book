<template>
  <article
    class="item-card"
    :class="[`source-line-${item.source}`]"
    :style="{ '--source-color': `var(--color-source-${sourceCssKey})` }"
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

    <!-- 翻译按钮区域 -->
    <div class="card-translate" @click.stop>
      <div class="translate-divider"></div>
      <button
        class="translate-btn"
        :disabled="translating"
        @click="handleTranslate"
      >
        {{ translateButtonText }}
      </button>
    </div>
  </article>
</template>

<script setup lang="ts">
import { ref, computed } from 'vue'
import type { Item } from '../api/client'
import { api } from '../api/client'

const props = defineProps<{
  item: Item
}>()

// 翻译状态
const translating = ref(false)
const isTranslated = ref(false)
const originalTitle = ref<string | null>(null)
const originalSummary = ref<string | null>(null)

const SOURCE_LABELS: Record<string, string> = {
  arxiv: 'arXiv',
  semantic_scholar: 'Semantic Scholar',
  dblp: 'DBLP',
  huggingface: 'HuggingFace',
  blog: '博客',
  github: 'GitHub',
}

// 将 source 转换为 CSS 变量键名（处理 semantic_scholar -> semantic）
const sourceCssKey = computed(() => {
  return props.item.source === 'semantic_scholar' ? 'semantic' : props.item.source
})

const sourceLabel = SOURCE_LABELS[props.item.source] || props.item.source

// 翻译按钮文本
const translateButtonText = computed(() => {
  if (translating.value) return '翻译中...'
  return isTranslated.value ? '🌐 原文' : '🌐 翻译'
})

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

// 翻译处理函数
async function handleTranslate() {
  // 如果已翻译，切回原文
  if (isTranslated.value && originalTitle.value !== null) {
    props.item.title = originalTitle.value
    props.item.summary = originalSummary.value || ''
    isTranslated.value = false
    return
  }

  // 保存原文
  if (originalTitle.value === null) {
    originalTitle.value = props.item.title
    originalSummary.value = props.item.summary
  }

  // 调用翻译 API
  translating.value = true
  try {
    const textToTranslate = [props.item.title, props.item.summary]
      .filter(Boolean)
      .join('\n')
    const result = await api.translate(textToTranslate)
    const translated = result.translated_text

    // 按换行符分割，第一行是标题，其余是摘要
    const lines = translated.split('\n')
    if (lines.length > 0) {
      props.item.title = lines[0]
      props.item.summary = lines.slice(1).join('\n') || props.item.summary
    }
    isTranslated.value = true
  } catch (error) {
    console.error('翻译失败:', error)
  } finally {
    translating.value = false
  }
}
</script>

<style scoped>
.item-card {
  position: relative;
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
  transform: translateY(-2px);
}

/* 时间线节点圆点 */
.item-card::before {
  content: '';
  position: absolute;
  left: -28px;
  top: 24px;
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: var(--source-color, var(--color-brand));
  border: 2px solid var(--color-surface, #fff);
  box-shadow: 0 0 0 2px var(--source-color, var(--color-brand));
}

/* 底部标尺线 */
.item-card::after {
  content: '';
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  height: 1px;
  background: color-mix(
    in srgb,
    var(--source-color, var(--color-brand)) 10%,
    transparent
  );
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

/* 翻译按钮区域 */
.card-translate {
  margin-top: var(--space-3, 12px);
  padding-top: var(--space-2, 8px);
}

.translate-divider {
  height: 1px;
  background: var(--color-border-light, #EEF0F4);
  margin-bottom: var(--space-2, 8px);
}

.translate-btn {
  display: inline-flex;
  align-items: center;
  padding: var(--space-1, 4px) var(--space-3, 12px);
  font-family: var(--font-body);
  font-size: var(--text-xs, 0.75rem);
  font-weight: var(--weight-medium, 500);
  color: var(--color-text-tertiary, #9CA3AF);
  background: transparent;
  border: 1px solid var(--color-border, #E2E5EC);
  border-radius: var(--radius-sm, 4px);
  cursor: pointer;
  transition: all var(--transition-fast, 150ms);
}

.translate-btn:hover:not(:disabled) {
  background: var(--color-surface-hover, #F3F4F6);
  color: var(--color-text-secondary, #5A6178);
}

.translate-btn:disabled {
  cursor: not-allowed;
  opacity: 0.6;
}
</style>