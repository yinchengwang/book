<template>
  <div v-if="selectedIds.length > 0" class="batch-actions">
    <span class="batch-count">已选 {{ selectedIds.length }} 项</span>
    <button class="btn btn-primary btn-sm" @click="batchComplete">✅ 完成</button>
    <button class="btn btn-secondary btn-sm" @click="batchDelete">🗑️ 删除</button>
    <button class="btn-text btn-sm" @click="$emit('clear')">取消选择</button>
  </div>
</template>

<script setup>
import { inject } from 'vue'
import api from '../api.js'

const props = defineProps({ selectedIds: Array })
const emit = defineEmits(['clear', 'done'])
const showToast = inject('showToast')

async function batchComplete() {
  const r = await api.batchUpdate(props.selectedIds, { status: 'closed' })
  if (r.code === 0) {
    showToast(`已完成 ${props.selectedIds.length} 项`)
    emit('done')
  } else showToast(r.msg || '操作失败', 'error')
}

async function batchDelete() {
  if (!confirm(`确定删除 ${props.selectedIds.length} 项？`)) return
  const r = await api.batchDelete(props.selectedIds)
  if (r.code === 0) {
    showToast(`已删除 ${props.selectedIds.length} 项`)
    emit('done')
  } else showToast(r.msg || '操作失败', 'error')
}
</script>

<style scoped>
.batch-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 16px;
  background: var(--primary-color, #4A90D9);
  color: white;
  border-radius: 6px;
  margin-bottom: 8px;
}
.batch-count {
  font-size: 13px;
  font-weight: 500;
  margin-right: 8px;
}
.batch-actions .btn { background: rgba(255,255,255,0.2); color: white; border: 1px solid rgba(255,255,255,0.3); }
.batch-actions .btn:hover { background: rgba(255,255,255,0.3); }
.batch-actions .btn-text { color: rgba(255,255,255,0.8); }
</style>