<template>
  <div class="sort-builder">
    <div class="sort-header">
      <span class="sort-title">排序规则</span>
      <button class="btn-text btn-sm" @click="addRule">+ 添加</button>
    </div>
    <div v-for="(rule, i) in localRules" :key="i" class="sort-rule">
      <select v-model="rule.field_id" class="form-select sort-field" @change="onChange">
        <option v-for="f in fields" :key="f.id" :value="f.id">{{ f.name }}</option>
      </select>
      <select v-model="rule.direction" class="form-select sort-dir" @change="onChange">
        <option value="asc">升序</option>
        <option value="desc">降序</option>
      </select>
      <button class="btn-text btn-sm" @click="moveUp(i)" :disabled="i === 0">↑</button>
      <button class="btn-text btn-sm" @click="moveDown(i)" :disabled="i === localRules.length - 1">↓</button>
      <button class="btn-text btn-sm sort-remove" @click="removeRule(i)">×</button>
    </div>
    <div v-if="localRules.length === 0" class="sort-empty">暂无排序规则</div>
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
  localRules.value.push({ field_id: 3, direction: 'asc' })
  emitValue()
}

function removeRule(i) { localRules.value.splice(i, 1); emitValue() }
function moveUp(i) {
  if (i === 0) return
  const tmp = localRules.value[i - 1]
  localRules.value[i - 1] = localRules.value[i]
  localRules.value[i] = tmp
  emitValue()
}
function moveDown(i) {
  if (i === localRules.value.length - 1) return
  const tmp = localRules.value[i + 1]
  localRules.value[i + 1] = localRules.value[i]
  localRules.value[i] = tmp
  emitValue()
}
function onChange() { emitValue() }
function emitValue() {
  emit('update:modelValue', localRules.value.map(r => ({ ...r })))
}
</script>

<style scoped>
.sort-builder { padding: 8px 0; }
.sort-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
.sort-title { font-weight: 500; font-size: 13px; color: #666; }
.sort-rule { display: flex; gap: 6px; align-items: center; margin-bottom: 6px; }
.sort-field { width: 120px; }
.sort-dir { width: 80px; }
.sort-remove { color: #e74c3c; }
.sort-empty { font-size: 12px; color: #aaa; padding: 8px 0; }
</style>