<template>
  <div :class="['todo-card', 'card', 'priority-' + todo.priority, todo.status === 'closed' ? 'is-closed' : '']" @click="$emit('click')">
    <div class="todo-actions">
      <button class="action-btn done" @click.stop="$emit('toggle')" title="完成">✓</button>
      <button class="action-btn delete" @click.stop="$emit('delete')" title="删除">×</button>
    </div>
    <div class="todo-card-header">
      <span :class="['priority-badge', 'priority-' + todo.priority]">
        {{ priorityLabel }}
      </span>
      <span class="todo-id">#{{ todo.id }}</span>
    </div>
    <div class="todo-title">{{ todo.title }}</div>
    <div class="todo-meta">
      <span v-if="todo.due_date && todo.due_date > 0" :class="['due-date', isOverdue ? 'overdue' : '']">
        📅 {{ formatDate(todo.due_date) }}
      </span>
      <span v-for="label in labels" :key="label" class="label">{{ label }}</span>
    </div>
    <div v-if="todo.checklist && todo.checklist.length" class="todo-progress">
      <span>{{ doneCount }}/{{ todo.checklist.length }}</span>
      <div class="progress-bar">
        <div class="progress-bar-fill" :style="{ width: progressPct + '%' }"></div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({ todo: Object })
defineEmits(['click', 'toggle', 'delete'])

const PRIORITY_LABELS = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']
const priorityLabel = computed(() => PRIORITY_LABELS[props.todo.priority] || '⚪无')

const labels = computed(() => {
  if (!props.todo.labels) return []
  if (Array.isArray(props.todo.labels)) return props.todo.labels
  try { return JSON.parse(props.todo.labels) } catch { return [] }
})

const doneCount = computed(() =>
  (props.todo.checklist || []).filter(i => i.done).length
)
const progressPct = computed(() =>
  props.todo.checklist?.length ? Math.round(doneCount.value * 100 / props.todo.checklist.length) : 0
)

const isOverdue = computed(() =>
  props.todo.due_date > 0 && props.todo.due_date < (Date.now() / 1000) && props.todo.status === 'open'
)

function formatDate(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getMonth()+1}/${d.getDate()}`
}
</script>

<style scoped>
.todo-card {
  cursor: pointer;
  transition: transform var(--transition-fast, 0.1s), box-shadow var(--transition-fast, 0.1s), border-color var(--transition-fast, 0.1s);
  position: relative;
  padding-left: 20px;
}

.todo-card::before {
  content: '';
  position: absolute;
  left: 0;
  top: 0;
  bottom: 0;
  width: 4px;
  border-radius: var(--radius) 0 0 var(--radius);
}

.todo-card.priority-0::before { background: var(--danger); }
.todo-card.priority-1::before { background: var(--warning); }
.todo-card.priority-2::before { background: var(--primary); }
.todo-card.priority-3::before { background: var(--success); }
.todo-card.priority-4::before { background: var(--text-muted); }

.todo-card:hover {
  transform: translateY(-2px);
  box-shadow: var(--glow-primary, 0 0 20px rgba(47, 129, 247, 0.3));
  border-color: var(--primary);
}

.todo-card.is-closed { opacity: 0.6; }
.todo-card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
.priority-badge { font-size: 12px; }
.todo-id { color: var(--text-muted); font-size: 12px; }
.todo-title { font-weight: 600; margin-bottom: 8px; word-break: break-word; }
.is-closed .todo-title { text-decoration: line-through; color: var(--text-muted); }
.todo-meta { display: flex; gap: 6px; align-items: center; flex-wrap: wrap; margin-bottom: 8px; }
.due-date { font-size: 12px; color: var(--text-muted); }
.due-date.overdue { color: var(--danger); }
.todo-progress { display: flex; align-items: center; gap: 8px; font-size: 12px; color: var(--text-muted); }
.progress-bar { flex: 1; height: 4px; background: var(--bg); border-radius: 2px; overflow: hidden; }
.progress-bar-fill { height: 100%; background: var(--success); transition: width var(--transition-normal, 0.2s); }
.progress-bar-fill[style*="100"] { background: var(--success); }
.progress-bar-fill[style*="0"] { background: var(--text-muted); }

/* 快捷操作按钮 */
.todo-actions {
  position: absolute;
  top: 8px;
  right: 8px;
  display: flex;
  gap: 4px;
  opacity: 0;
  transition: opacity var(--transition-fast, 0.1s);
}
.todo-card:hover .todo-actions { opacity: 1; }
.action-btn {
  width: 24px;
  height: 24px;
  border-radius: 4px;
  border: none;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 12px;
  transition: background var(--transition-fast, 0.1s);
}
.action-btn.done { background: var(--success); color: white; }
.action-btn.delete { background: var(--danger); color: white; }
</style>
