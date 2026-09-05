<template>
  <div class="inline-edit" @dblclick="enterEdit">
    <input
      v-if="editing"
      ref="inputRef"
      :value="draft"
      class="inline-edit-input"
      maxlength="255"
      data-test="inline-edit-input"
      @input="onInput"
      @keyup.enter="commit"
      @keyup.esc="cancel"
      @blur="commit"
    />
    <span v-else class="inline-edit-text" :title="modelValue">{{ modelValue }}</span>
  </div>
</template>

<script setup lang="ts">
import { nextTick, ref, watch } from 'vue'

const props = defineProps<{ modelValue: string }>()
const emit = defineEmits<{
  'update:modelValue': [value: string]
  commit: [value: string]
}>()

const editing = ref(false)
const draft = ref(props.modelValue)
const inputRef = ref<HTMLInputElement | null>(null)

watch(
  () => props.modelValue,
  (val) => {
    if (!editing.value) draft.value = val
  }
)

async function enterEdit(): Promise<void> {
  draft.value = props.modelValue
  editing.value = true
  await nextTick()
  inputRef.value?.focus()
  inputRef.value?.select()
}

function onInput(event: Event): void {
  const target = event.target as HTMLInputElement
  draft.value = target.value
  emit('update:modelValue', target.value)
}

function commit(): void {
  const trimmed = draft.value.trim()
  if (!trimmed) {
    cancel()
    return
  }
  if (trimmed !== props.modelValue) {
    emit('update:modelValue', trimmed)
    emit('commit', trimmed)
  }
  editing.value = false
}

function cancel(): void {
  draft.value = props.modelValue
  emit('update:modelValue', props.modelValue)
  editing.value = false
}
</script>

<style scoped>
.inline-edit { display: inline-block; min-width: 0; max-width: 100%; }
.inline-edit-text {
  cursor: text;
  word-break: break-word;
  border-bottom: 1px dashed transparent;
  transition: border-color var(--transition-fast, 0.1s);
}
.inline-edit-text:hover { border-bottom-color: var(--border); }
.inline-edit-input {
  width: 100%;
  font-size: inherit;
  font-weight: inherit;
  padding: 4px 6px;
  border: 1px solid var(--primary);
  border-radius: 4px;
  background: var(--bg-elev);
  color: var(--text);
}
</style>
