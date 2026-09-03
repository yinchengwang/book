<template>
  <div v-if="todo" class="detail-panel">
    <button class="close-btn" @click="$emit('close')">×</button>
    <div class="detail-header">
      <span :class="['priority-badge', 'priority-' + todo.priority]">{{ priorityLabel }}</span>
      <h2>{{ todo.title }}</h2>
      <span class="todo-id">#{{ todo.id }}</span>
    </div>
    <div class="detail-body">
      <p v-if="todo.description" class="description">{{ todo.description }}</p>
      <p v-else class="description empty-desc">暂无描述</p>
      <div class="todo-meta">
        <span v-if="todo.due_date && todo.due_date > 0" :class="['due-date', isOverdue ? 'overdue' : '']">
          📅 截止：{{ formatDate(todo.due_date) }}
        </span>
        <span v-for="label in labels" :key="label" class="label">{{ label }}</span>
      </div>
      <Checklist :todo-id="todo.id" :items="todo.checklist || []" @updated="$emit('updated')" />
      <Comments :todo-id="todo.id" :comments="todo.comments || []" @updated="$emit('updated')" />
      <div class="detail-actions">
        <button class="btn btn-secondary btn-sm" @click="createChange">🔀 创建 OPSX 变更</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, inject } from 'vue'
import api from '../api.js'
import Checklist from './Checklist.vue'
import Comments from './Comments.vue'

const props = defineProps({ todo: Object })
defineEmits(['updated', 'close'])
const showToast = inject('showToast')

const PRIORITY_LABELS = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']
const priorityLabel = computed(() => PRIORITY_LABELS[props.todo?.priority] || '⚪无')
const labels = computed(() => {
  if (!props.todo?.labels) return []
  if (Array.isArray(props.todo.labels)) return props.todo.labels
  try { return JSON.parse(props.todo.labels) } catch { return [] }
})
const isOverdue = computed(() => props.todo?.due_date > 0 && props.todo?.due_date < (Date.now() / 1000))
function formatDate(ts) { return new Date(ts * 1000).toLocaleDateString() }

async function createChange() {
  const r = await api.createChange(props.todo.id)
  if (r.code === 0) showToast(`变更已创建: ${r.data.change_id}`)
  else showToast(r.msg || '创建变更失败', 'error')
}
</script>

<style scoped>
.detail-panel { padding: 24px; overflow-y: auto; position: relative; animation: slideInRight var(--transition-normal, 0.2s) ease-out; }
.detail-header { margin-bottom: 16px; padding-bottom: 16px; border-bottom: 1px solid var(--border); }
.detail-header h2 { margin: 8px 0; font-size: 20px; }
.todo-id { color: var(--text-muted); font-size: 12px; }
.description { background: var(--bg); padding: 12px; border-radius: var(--radius); white-space: pre-wrap; margin-bottom: 12px; border: 1px solid var(--border); }
.empty-desc { color: var(--text-muted); font-style: italic; }
.todo-meta { display: flex; gap: 6px; align-items: center; margin-bottom: 16px; flex-wrap: wrap; }
.detail-actions { margin-top: 16px; padding-top: 16px; border-top: 1px solid var(--border); }

.close-btn {
  position: absolute;
  top: 16px;
  right: 16px;
  width: 32px;
  height: 32px;
  border-radius: 50%;
  border: none;
  background: var(--bg-elev2);
  color: var(--text-muted);
  cursor: pointer;
  font-size: 18px;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: background var(--transition-fast, 0.1s), color var(--transition-fast, 0.1s);
}
.close-btn:hover { background: var(--danger); color: white; }
</style>
