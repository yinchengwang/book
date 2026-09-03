<template>
  <div class="comments">
    <h4>评论 ({{ items.length }})</h4>
    <div v-for="c in items" :key="c.id" class="comment">
      <span class="comment-text">{{ c.text }}</span>
      <span class="comment-time">{{ formatTime(c.created_at) }}</span>
      <button class="btn-icon" @click="remove(c.id)">×</button>
    </div>
    <div class="comment-add">
      <input v-model.trim="newText" class="form-input" placeholder="添加评论..." @keyup.enter="add" />
      <button class="btn btn-secondary btn-sm" @click="add" :disabled="!newText">发送</button>
    </div>
  </div>
</template>

<script setup>
import { ref, inject } from 'vue'
import api from '../api.js'

const props = defineProps({ 'todo-id': Number, items: Array })
const emit = defineEmits(['updated'])
const showToast = inject('showToast')
const newText = ref('')

async function add() {
  if (!newText.value) return
  const r = await api.addComment(props.todoId, newText.value)
  if (r.code === 0) { emit('updated'); newText.value = '' }
  else showToast(r.msg || '发送失败', 'error')
}
async function remove(cid) {
  const r = await api.deleteComment(props.todoId, cid)
  if (r.code === 0) emit('updated')
}
function formatTime(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getMonth()+1}/${d.getDate()} ${d.getHours()}:${String(d.getMinutes()).padStart(2,'0')}`
}
</script>

<style scoped>
.comments { margin: 16px 0; padding-top: 16px; border-top: 1px solid var(--border); }
.comments h4 { margin-bottom: 8px; font-size: 14px; }
.comment { display: flex; align-items: center; gap: 8px; padding: 8px 0; border-bottom: 1px solid var(--border); }
.comment-text { flex: 1; }
.comment-time { font-size: 12px; color: var(--text-muted); }
.comment-add { display: flex; gap: 8px; margin-top: 8px; }
.btn-icon { background: none; border: none; color: var(--text-muted); cursor: pointer; font-size: 18px; }
.btn-icon:hover { color: var(--danger); }
</style>
