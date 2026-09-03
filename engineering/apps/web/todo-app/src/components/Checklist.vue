<template>
  <div class="checklist">
    <h4>待办项 {{ doneCount }}/{{ items.length }}</h4>
    <div v-for="item in items" :key="item.id" class="checklist-item">
      <input type="checkbox" :checked="item.done" @change="toggle(item.id)" />
      <span :class="item.done ? 'done' : ''">{{ item.text }}</span>
      <button class="btn-icon" @click="remove(item.id)">×</button>
    </div>
    <div class="checklist-add">
      <input v-model.trim="newText" class="form-input" placeholder="新增待办项..." @keyup.enter="add" />
      <button class="btn btn-secondary btn-sm" @click="add" :disabled="!newText">添加</button>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, inject } from 'vue'
import api from '../api.js'

const props = defineProps({ 'todo-id': Number, items: Array })
const emit = defineEmits(['updated'])
const showToast = inject('showToast')
const newText = ref('')
const doneCount = computed(() => props.items.filter(i => i.done).length)

async function add() {
  if (!newText.value) return
  const r = await api.addChecklist(props.todoId, newText.value)
  if (r.code === 0) { emit('updated'); newText.value = '' }
  else showToast(r.msg || '添加失败', 'error')
}
async function toggle(itemId) {
  const r = await api.toggleChecklist(props.todoId, itemId)
  if (r.code === 0) emit('updated')
}
async function remove(itemId) {
  if (!confirm('确认删除?')) return
  const r = await api.removeChecklist(props.todoId, itemId)
  if (r.code === 0) emit('updated')
}
</script>

<style scoped>
.checklist { margin: 16px 0; padding-top: 16px; border-top: 1px solid var(--border); }
.checklist h4 { margin-bottom: 8px; font-size: 14px; }
.checklist-item { display: flex; align-items: center; gap: 8px; padding: 4px 0; }
.checklist-item span { flex: 1; }
.checklist-item span.done { text-decoration: line-through; color: var(--text-muted); }
.checklist-add { display: flex; gap: 8px; margin-top: 8px; }
.btn-icon { background: none; border: none; color: var(--text-muted); cursor: pointer; font-size: 18px; }
.btn-icon:hover { color: var(--danger); }
</style>
