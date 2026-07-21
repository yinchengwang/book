<template>
  <span class="field-cell-display">
    <!-- 数字类型 -->
    <span v-if="field.type === 'number'" class="field-number">{{ displayValue }}</span>

    <!-- 单选 -->
    <span v-else-if="field.type === 'single_select'" class="field-select">
      <span class="select-tag">{{ displayValue }}</span>
    </span>

    <!-- 多选 -->
    <span v-else-if="field.type === 'multi_select'" class="field-multi-select">
      <span v-for="opt in selectedValues" :key="opt" class="multi-tag">{{ opt }}</span>
    </span>

    <!-- 日期 -->
    <span v-else-if="field.type === 'date'" class="field-date">{{ displayValue }}</span>

    <!-- 日期时间 -->
    <span v-else-if="field.type === 'datetime'" class="field-datetime">{{ displayValue }}</span>

    <!-- 文本及其他 -->
    <span v-else class="field-text">{{ value }}</span>
  </span>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  field: Object,
  value: [String, Number]
})

const displayValue = computed(() => {
  if (!props.value && props.value !== 0) return '-'

  if (props.field.type === 'number') {
    return props.value
  }

  if (props.field.type === 'date' || props.field.type === 'datetime') {
    const ts = parseInt(props.value)
    if (!ts) return '-'
    const d = new Date(ts * 1000)
    if (props.field.type === 'date') {
      return d.toLocaleDateString('zh-CN')
    }
    return d.toLocaleString('zh-CN')
  }

  return props.value
})

const selectedValues = computed(() => {
  if (!props.value) return []
  if (props.field.type !== 'multi_select') return []
  try { return JSON.parse(props.value) }
  catch { return [props.value] }
})
</script>

<style scoped>
.field-cell-display {
  display: inline-flex;
  align-items: center;
  gap: 2px;
  max-width: 100%;
}
.field-number {
  font-variant-numeric: tabular-nums;
  color: #1976d2;
}
.select-tag {
  display: inline-block;
  padding: 1px 8px;
  border-radius: 4px;
  font-size: 12px;
  background: #e3f2fd;
  color: #1976d2;
}
.multi-tag {
  display: inline-block;
  padding: 1px 6px;
  margin: 1px 2px;
  border-radius: 4px;
  font-size: 11px;
  background: #f3e5f5;
  color: #7b1fa2;
}
.field-date, .field-datetime {
  color: #555;
  font-size: 13px;
}
.field-text {
  color: #333;
}
</style>