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

      <!-- 扩展字段 -->
      <div v-if="extFieldValues.length > 0" class="ext-fields-section">
        <h4 class="section-title">扩展字段</h4>
        <div v-for="item in extFieldValues" :key="item.id" class="ext-field-row">
          <span class="ext-field-name">{{ item.name }}：</span>
          <span class="ext-field-value">{{ item.value }}</span>
          <button v-if="!item.built_in" class="btn-text btn-xs" @click="editField(item)">✏️</button>
        </div>
      </div>
      <div v-else class="ext-fields-section">
        <h4 class="section-title">扩展字段</h4>
        <p class="empty-desc">暂无扩展字段</p>
      </div>

      <!-- 编辑扩展字段弹窗 -->
      <div v-if="editingField" class="ext-edit-overlay" @click.self="editingField = null">
        <div class="ext-edit-box">
          <h4>编辑：{{ editingField.name }}</h4>
          <input
            v-model="editValue"
            class="form-input"
            :placeholder="'输入' + editingField.name"
          />
          <div style="display:flex; gap:8px; margin-top:8px;">
            <button class="btn btn-primary btn-sm" @click="saveFieldValue">保存</button>
            <button class="btn btn-secondary btn-sm" @click="editingField = null">取消</button>
          </div>
        </div>
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
import { computed, ref, inject, watch } from 'vue'
import api from '../api.js'
import Checklist from './Checklist.vue'
import Comments from './Comments.vue'

const props = defineProps({ todo: Object })
const emit = defineEmits(['updated', 'close'])
const showToast = inject('showToast')

const PRIORITY_LABELS = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']
const priorityLabel = computed(() => PRIORITY_LABELS[props.todo?.priority] || '⚪无')
const labels = computed(() => {
  if (!props.todo?.labels) return []
  if (Array.isArray(props.todo.labels)) return props.todo.labels
  try { return JSON.parse(props.todo.labels) } catch { return [] }
})
const isOverdue = computed(() => props.todo?.due_date > 0 && props.todo?.due_date < (Date.now() / 1000))

const fields = ref([])
const extFieldValues = ref([])
const editingField = ref(null)
const editValue = ref('')

// 加载字段定义并匹配 todo 的扩展字段值
watch(() => props.todo, async (todo) => {
  if (!todo) return
  extFieldValues.value = []
  const res = await api.listFields()
  if (res.code === 0) {
    fields.value = res.data
    const todoFields = todo.fields || {}
    extFieldValues.value = res.data
      .filter(f => f.id >= 10 || todoFields[f.id])
      .map(f => ({
        id: f.id,
        name: f.name,
        type: f.type,
        built_in: f.built_in,
        value: todoFields[f.id] || '-'
      }))
  }
}, { immediate: true })

function formatDate(ts) { return new Date(ts * 1000).toLocaleDateString() }

async function createChange() {
  const r = await api.createChange(props.todo.id)
  if (r.code === 0) showToast(`变更已创建: ${r.data.change_id}`)
  else showToast(r.msg || '创建变更失败', 'error')
}

function editField(field) {
  editingField.value = field
  editValue.value = field.value === '-' ? '' : field.value
}

async function saveFieldValue() {
  if (!editingField.value || !props.todo) return
  const fields = {}
  fields[String(editingField.value.id)] = editValue.value
  const r = await api.updateTodoFields(props.todo.id, fields)
  if (r.code === 0) {
    showToast('字段已更新')
    editingField.value = null
    emit('updated')
  } else {
    showToast(r.msg || '更新失败', 'error')
  }
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

.ext-fields-section {
  margin: 12px 0;
  padding: 12px;
  background: var(--bg-elev, #f8f9fa);
  border-radius: var(--radius, 6px);
}
.section-title {
  font-size: 13px;
  font-weight: 600;
  margin: 0 0 8px 0;
  color: var(--text-muted, #666);
}
.ext-field-row {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 0;
  border-bottom: 1px solid var(--border, #eee);
  font-size: 13px;
}
.ext-field-row:last-child { border-bottom: none; }
.ext-field-name {
  font-weight: 500;
  color: var(--text-muted, #666);
  min-width: 60px;
}
.ext-field-value {
  flex: 1;
}
.btn-xs { font-size: 12px; padding: 2px 4px; }
.ext-edit-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0,0,0,0.3);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
}
.ext-edit-box {
  background: var(--card-bg, #fff);
  padding: 20px;
  border-radius: 8px;
  width: 360px;
  max-width: 90vw;
  box-shadow: 0 4px 16px rgba(0,0,0,0.2);
}
.ext-edit-box h4 { margin: 0 0 12px 0; }

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