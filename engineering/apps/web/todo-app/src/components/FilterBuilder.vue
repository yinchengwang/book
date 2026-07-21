<template>
  <div class="filter-builder">
    <div class="filter-header">
      <span class="filter-title">筛选条件</span>
      <button class="btn-text btn-sm" @click="addRule">+ 添加</button>
    </div>
    <div v-for="(rule, i) in localRules" :key="i" class="filter-rule">
      <select v-model="rule.field_id" class="form-select filter-field" @change="onChange">
        <option v-for="f in fields" :key="f.id" :value="f.id">{{ f.name }}</option>
      </select>
      <select v-model="rule.operator" class="form-select filter-op" @change="onChange">
        <option value="eq">等于</option>
        <option value="ne">不等于</option>
        <option value="gt">大于</option>
        <option value="lt">小于</option>
        <option value="gte">大于等于</option>
        <option value="lte">小于等于</option>
        <option value="contains">包含</option>
        <option value="is_empty">为空</option>
        <option value="is_not_empty">不为空</option>
      </select>
      <input
        v-if="!['is_empty','is_not_empty'].includes(rule.operator)"
        v-model="rule.value"
        class="form-input filter-value"
        placeholder="值"
        @input="onChange"
      />
      <button class="btn-text btn-sm filter-remove" @click="removeRule(i)">×</button>
    </div>
    <div v-if="localRules.length === 0" class="filter-empty">暂无筛选条件</div>
  </div>
</template>

<script setup>
import { ref, watch } from 'vue'

const props = defineProps({
  fields: Array,
  modelValue: Array
})
const emit = defineEmits(['update:modelValue'])

const localRules = ref([])

watch(() => props.modelValue, (v) => {
  localRules.value = (v || []).map(r => ({ ...r }))
}, { immediate: true, deep: true })

function addRule() {
  localRules.value.push({ field_id: 3, operator: 'eq', value: '' })
  emitValue()
}

function removeRule(i) {
  localRules.value.splice(i, 1)
  emitValue()
}

function onChange() {
  emitValue()
}

function emitValue() {
  emit('update:modelValue', localRules.value.map(r => ({ ...r })))
}
</script>

<style scoped>
.filter-builder { padding: 8px 0; }
.filter-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
.filter-title { font-weight: 500; font-size: 13px; color: #666; }
.filter-rule { display: flex; gap: 6px; align-items: center; margin-bottom: 6px; }
.filter-field { width: 120px; }
.filter-op { width: 100px; }
.filter-value { flex: 1; }
.filter-remove { color: #e74c3c; }
.filter-empty { font-size: 12px; color: #aaa; padding: 8px 0; }
</style>