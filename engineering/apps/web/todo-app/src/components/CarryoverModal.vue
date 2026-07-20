<template>
  <div class="modal-overlay" v-if="show" @click.self="$emit('close')">
    <div class="modal">
      <div class="modal-header">
        <h3>📋 过期任务确认</h3>
        <button class="close-btn" @click="$emit('close')">✕</button>
      </div>
      <p>您有 {{ localTasks.length }} 个任务已过期，请确认是否已完成：</p>
      <div class="task-list">
        <div v-for="task in localTasks" :key="task.id" class="task-item">
          <label>
            <input type="checkbox" v-model="task.completed" />
            <span class="task-title">{{ task.title }}</span>
            <span class="task-date">预期：{{ formatDate(task.due_date) }}</span>
          </label>
        </div>
      </div>
      <div class="modal-actions">
        <button class="btn btn-secondary" @click="markAllDone">全部标记已完成</button>
        <button class="btn btn-primary" @click="confirm">确认</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch } from 'vue'

const props = defineProps({ show: Boolean, tasks: Array })
const emit = defineEmits(['close', 'confirm'])

const localTasks = ref([])
watch(() => props.tasks, (t) => {
  localTasks.value = (t || []).map(t => ({ ...t, completed: false }))
}, { immediate: true })

function markAllDone() {
  localTasks.value.forEach(t => t.completed = true)
}

function confirm() {
  emit('confirm', localTasks.value.map(t => ({ todo_id: t.id, completed: t.completed ? 1 : 0 })))
}

function formatDate(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getMonth() + 1}/${d.getDate()}`
}
</script>

<style scoped>
.modal-overlay { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.4); display: flex; align-items: center; justify-content: center; z-index: 1000; }
.modal { background: #fff; border-radius: 8px; padding: 24px; min-width: 480px; max-height: 80vh; overflow-y: auto; box-shadow: 0 4px 20px rgba(0,0,0,0.15); }
.modal-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; }
.modal-header h3 { margin: 0; }
.close-btn { background: none; border: none; font-size: 1.2em; cursor: pointer; color: #999; }
.close-btn:hover { color: #333; }
.task-list { margin: 16px 0; }
.task-item { padding: 8px 0; border-bottom: 1px solid #eee; }
.task-item label { display: flex; align-items: center; gap: 8px; cursor: pointer; }
.task-title { flex: 1; }
.task-date { color: #999; font-size: 0.85em; }
.modal-actions { display: flex; gap: 8px; justify-content: flex-end; margin-top: 16px; }
.btn { padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
.btn-primary { background: #0078D4; color: #fff; }
.btn-primary:hover { background: #106EBE; }
.btn-secondary { background: #f0f0f0; color: #333; }
.btn-secondary:hover { background: #e0e0e0; }
</style>