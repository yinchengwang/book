<template>
  <div v-if="todo" class="modal-mask" data-test="edit-todo-dialog" @click.self="close">
    <div class="modal">
      <h3>编辑待办</h3>
      <label class="label-text">标题 *</label>
      <input
        v-model.trim="form.title"
        class="form-input"
        maxlength="255"
        data-test="edit-title"
        @keyup.enter="save"
      />
      <label class="label-text">描述</label>
      <textarea
        v-model="form.description"
        class="form-textarea"
        rows="4"
        maxlength="4000"
        data-test="edit-description"
      ></textarea>
      <label class="label-text">优先级</label>
      <select v-model.number="form.priority" class="form-select" data-test="edit-priority">
        <option v-for="(label, i) in PRIORITY_LABELS" :key="i" :value="i">{{ label }}</option>
      </select>
      <label class="label-text">截止日期（Unix 时间戳，0=无）</label>
      <input
        v-model.number="form.due_date"
        type="number"
        class="form-input"
        placeholder="0"
        data-test="edit-due-date"
      />
      <label class="label-text">分组</label>
      <select v-model.number="form.group_id" class="form-select" data-test="edit-group">
        <option :value="0">未分组</option>
        <option v-for="g in groups" :key="g.id" :value="g.id">{{ g.name }}</option>
      </select>
      <label class="label-text">标签（逗号分隔）</label>
      <input
        v-model="form.labelsRaw"
        class="form-input"
        placeholder="bug, urgent"
        data-test="edit-labels"
      />
      <label class="label-text">状态</label>
      <select v-model="form.status" class="form-select" data-test="edit-status">
        <option value="open">未完成</option>
        <option value="done">已完成</option>
        <option value="closed">已关闭</option>
        <option value="archived">已归档</option>
      </select>
      <div
        v-if="errorMessage"
        class="dialog-error"
        data-test="edit-error"
        role="alert"
      >{{ errorMessage }}</div>
      <div class="dialog-actions">
        <button class="btn btn-danger" data-test="edit-delete" @click="onDelete">删除</button>
        <div class="spacer"></div>
        <button class="btn btn-secondary" data-test="edit-cancel" @click="close">取消</button>
        <button
          class="btn btn-primary"
          data-test="edit-save"
          :disabled="!form.title"
          @click="save"
        >保存</button>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { reactive, ref, watch } from 'vue'
import { useTodosStore } from '@/stores/todos'
import type { Todo, Group } from '@/types/models'

const PRIORITY_LABELS = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']

interface FormState {
  title: string
  description: string
  priority: number
  due_date: number
  group_id: number
  labelsRaw: string
  status: Todo['status']
}

const baseTodo = (): Todo => ({
  id: 0,
  title: '',
  status: 'open',
  priority: 4,
  due_date: 0,
  group_id: 0,
  labels: []
})

const props = defineProps<{
  modelValue: boolean
  todo: Todo | null
  groups: Group[]
}>()
const emit = defineEmits<{
  'update:modelValue': [value: boolean]
  updated: [todo: Todo]
  deleted: [id: number]
  error: [payload: { action: 'save' | 'delete'; message: string }]
}>()

const store = useTodosStore()
const errorMessage = ref<string | null>(null)

/**
 * Render `Todo.labels` (which the backend may serialize as either
 * `string[]` or a JSON `string`) into the comma-separated string the
 * form input expects. Preserves the value verbatim when it is already
 * a string so we never silently drop user data.
 */
function labelsToString(labels: Todo['labels']): string {
  if (Array.isArray(labels)) return labels.join(', ')
  if (typeof labels === 'string') return labels
  return ''
}

function makeForm(t: Todo): FormState {
  return {
    title: t.title,
    description: t.description ?? '',
    priority: t.priority,
    due_date: t.due_date,
    group_id: t.group_id,
    labelsRaw: labelsToString(t.labels),
    status: t.status
  }
}

const form = reactive<FormState>(makeForm(props.todo ?? baseTodo()))

watch(
  () => props.todo,
  (next) => {
    if (next) Object.assign(form, makeForm(next))
    errorMessage.value = null
  },
  { immediate: true }
)

function close(): void {
  errorMessage.value = null
  emit('update:modelValue', false)
}

async function save(): Promise<void> {
  if (!props.todo || !form.title) return
  errorMessage.value = null
  const labels = form.labelsRaw
    .split(',')
    .map((s) => s.trim())
    .filter(Boolean)
  try {
    const updated = await store.update(props.todo.id, {
      title: form.title,
      description: form.description,
      priority: form.priority,
      due_date: form.due_date,
      group_id: form.group_id,
      labels,
      status: form.status
    })
    emit('updated', updated)
    close()
  } catch (err) {
    const message = err instanceof Error ? err.message : '保存失败'
    errorMessage.value = message
    emit('error', { action: 'save', message })
  }
}

async function onDelete(): Promise<void> {
  if (!props.todo) return
  errorMessage.value = null
  try {
    await store.remove(props.todo.id)
    emit('deleted', props.todo.id)
    close()
  } catch (err) {
    const message = err instanceof Error ? err.message : '删除失败'
    errorMessage.value = message
    emit('error', { action: 'delete', message })
  }
}
</script>

<style scoped>
.dialog-actions {
  display: flex;
  gap: 8px;
  margin-top: 16px;
  align-items: center;
}
.dialog-actions .spacer { flex: 1; }
.dialog-error {
  color: var(--danger, #c0392b);
  background: var(--danger-bg, rgba(192, 57, 43, 0.08));
  border: 1px solid var(--danger-border, rgba(192, 57, 43, 0.25));
  border-radius: 4px;
  padding: 8px 12px;
  margin-top: 12px;
  font-size: 0.9em;
}
</style>
