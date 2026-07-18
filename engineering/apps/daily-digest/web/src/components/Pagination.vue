<template>
  <div class="pagination" v-if="totalPages > 1">
    <!-- 每页条数选择器 -->
    <select class="size-select" :value="size" @change="onSizeChange">
      <option v-for="opt in sizeOptions" :key="opt" :value="opt">
        {{ opt }}条/页
      </option>
    </select>

    <!-- 上一页按钮 -->
    <button
      class="nav-btn"
      :disabled="modelValue <= 1"
      @click="goPage(modelValue - 1)"
    >
      ← 上一页
    </button>

    <!-- 页码标签 -->
    <div class="page-numbers">
      <template v-for="(item, index) in pageLabels" :key="index">
        <button
          v-if="item !== '...'"
          class="page-num"
          :class="{ active: item === modelValue }"
          @click="goPage(item as number)"
        >
          {{ item }}
        </button>
        <span v-else class="ellipsis">...</span>
      </template>
    </div>

    <!-- 下一页按钮 -->
    <button
      class="nav-btn"
      :disabled="modelValue >= totalPages"
      @click="goPage(modelValue + 1)"
    >
      下一页 →
    </button>

    <!-- 页码信息 -->
    <span class="page-info">
      {{ modelValue }}/{{ totalPages }}
    </span>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

// 组件属性定义
const props = withDefaults(
  defineProps<{
    modelValue: number // 当前页码（双向绑定）
    total: number // 总条目数
    size: number // 每页条数
    sizeOptions?: number[] // 每页条数选项
  }>(),
  {
    sizeOptions: () => [10, 20, 50, 100],
  }
)

// 事件定义
const emit = defineEmits<{
  'update:modelValue': [page: number] // 页码变化
  'update:size': [size: number] // 每页条数变化
}>()

// 计算总页数
const totalPages = computed(() => Math.ceil(props.total / props.size) || 1)

/**
 * 生成页码标签数组
 * 规则：
 * - 总页数 <= 7：全部显示
 * - 当前页前后 2 页 + 首尾页始终可见
 * - 超出范围用 '...' 省略号
 */
const pageLabels = computed(() => {
  const total = totalPages.value
  const current = props.modelValue

  // 总页数 <= 7，全部显示
  if (total <= 7) {
    return Array.from({ length: total }, (_, i) => i + 1)
  }

  // 复杂情况：计算显示的页码
  const labels: (number | string)[] = []

  // 始终显示第 1 页
  labels.push(1)

  // 计算中间页码范围
  let start = Math.max(2, current - 2)
  let end = Math.min(total - 1, current + 2)

  // 左侧省略号
  if (start > 2) {
    labels.push('...')
  }

  // 中间页码
  for (let i = start; i <= end; i++) {
    labels.push(i)
  }

  // 右侧省略号
  if (end < total - 1) {
    labels.push('...')
  }

  // 始终显示最后一页
  labels.push(total)

  return labels
})

// 跳转到指定页
function goPage(page: number) {
  if (page >= 1 && page <= totalPages.value && page !== props.modelValue) {
    emit('update:modelValue', page)
  }
}

// 每页条数变化
function onSizeChange(e: Event) {
  const target = e.target as HTMLSelectElement
  const newSize = parseInt(target.value, 10)
  if (newSize !== props.size) {
    emit('update:size', newSize)
    // 每页条数变化时重置到第 1 页
    emit('update:modelValue', 1)
  }
}
</script>

<style scoped>
.pagination {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: var(--space-2, 8px);
  padding: var(--space-8, 32px) 0;
  flex-wrap: wrap;
}

/* 每页条数选择器 */
.size-select {
  padding: var(--space-2, 8px) var(--space-3, 12px);
  font-family: var(--font-body, sans-serif);
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-primary, #1A1A2E);
  background: var(--color-surface, #fff);
  border: 1px solid var(--color-border, #E2E5EC);
  border-radius: var(--radius-md, 8px);
  cursor: pointer;
  outline: none;
  transition: border-color var(--transition-fast, 150ms);
}

.size-select:hover {
  border-color: var(--color-brand, #2D7FF9);
}

.size-select:focus {
  border-color: var(--color-brand, #2D7FF9);
  box-shadow: 0 0 0 3px rgba(45, 127, 249, 0.15);
}

/* 导航按钮（上一页/下一页） */
.nav-btn {
  padding: var(--space-2, 8px) var(--space-3, 12px);
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-sm, 0.8125rem);
  font-weight: var(--weight-medium, 500);
  color: var(--color-text-primary, #1A1A2E);
  background: var(--color-surface, #fff);
  border: 1px solid var(--color-border, #E2E5EC);
  border-radius: var(--radius-md, 8px);
  cursor: pointer;
  transition: all var(--transition-fast, 150ms);
  white-space: nowrap;
}

.nav-btn:hover:not(:disabled) {
  border-color: var(--color-brand, #2D7FF9);
  color: var(--color-brand, #2D7FF9);
}

.nav-btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

/* 页码数字容器 */
.page-numbers {
  display: flex;
  align-items: center;
  gap: var(--space-1, 4px);
}

/* 页码按钮 */
.page-num {
  min-width: 32px;
  height: 32px;
  padding: 0 var(--space-2, 8px);
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-sm, 0.8125rem);
  font-weight: var(--weight-medium, 500);
  color: var(--color-text-primary, #1A1A2E);
  background: var(--color-surface, #fff);
  border: 1px solid var(--color-border, #E2E5EC);
  border-radius: var(--radius-md, 8px);
  cursor: pointer;
  transition: all var(--transition-fast, 150ms);
}

.page-num:hover:not(.active) {
  border-color: var(--color-brand, #2D7FF9);
  color: var(--color-brand, #2D7FF9);
}

/* 当前页激活状态 */
.page-num.active {
  color: #fff;
  background: var(--color-brand, #2D7FF9);
  border-color: var(--color-brand, #2D7FF9);
}

/* 省略号 */
.ellipsis {
  min-width: 32px;
  height: 32px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-tertiary, #9CA3AF);
  cursor: default;
}

/* 页码信息 */
.page-info {
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-tertiary, #9CA3AF);
  margin-left: var(--space-2, 8px);
}
</style>
