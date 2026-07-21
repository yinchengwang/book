<template>
  <div class="field-cell">
    <input v-if="field.type === 'number'" type="number" :value="value" @input="onInput($event.target.value)" class="cell-input" />
    <select v-else-if="field.type === 'single_select'" :value="value" @change="onInput($event.target.value)" class="cell-input">
      <option value="">未选择</option>
      <option v-for="opt in choices" :key="opt" :value="opt">{{ opt }}</option>
    </select>
    <div v-else-if="field.type === 'multi_select'" class="multi-select">
      <label v-for="opt in choices" :key="opt" class="checkbox-label">
        <input type="checkbox" :checked="selectedValues.includes(opt)" @change="toggleMulti(opt)" />
        {{ opt }}
      </label>
    </div>
    <input v-else-if="field.type === 'date'" type="date" :value="dateValue" @input="onDateInput($event.target.value)" class="cell-input" />
    <input v-else type="text" :value="value" @input="onInput($event.target.value)" class="cell-input" />
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  field: Object,
  modelValue: [String, Number]
})
const emit = defineEmits(['update:modelValue'])

const choices = computed(() => {
  if (!props.field.options) return []
  try {
    const opts = JSON.parse(props.field.options)
    return opts.choices || []
  } catch { return [] }
})

const selectedValues = computed(() => {
  if (!props.modelValue) return []
  try { return JSON.parse(props.modelValue) }
  catch { return [props.modelValue] }
})

const dateValue = computed(() => {
  if (!props.modelValue) return ''
  const d = new Date(parseInt(props.modelValue) * 1000)
  return d.toISOString().slice(0, 10)
})

function onInput(v) { emit('update:modelValue', v) }
function toggleMulti(opt) {
  const arr = [...selectedValues.value]
  const idx = arr.indexOf(opt)
  if (idx >= 0) arr.splice(idx, 1)
  else arr.push(opt)
  emit('update:modelValue', JSON.stringify(arr))
}
function onDateInput(v) {
  if (!v) { emit('update:modelValue', ''); return }
  const ts = Math.floor(new Date(v + 'T00:00:00').getTime() / 1000)
  emit('update:modelValue', String(ts))
}
</script>

<style scoped>
.cell-input { width: 100%; border: 1px solid #ddd; border-radius: 2px; padding: 2px 4px; font-size: inherit; }
.multi-select { display: flex; flex-direction: column; gap: 2px; }
.checkbox-label { display: flex; align-items: center; gap: 4px; font-size: 12px; cursor: pointer; }
</style>