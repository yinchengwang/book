<template>
  <div class="group-manager">
    <div class="manager-header">
      <h2>📁 分组管理</h2>
      <button class="btn btn-primary" @click="showCreate = true">+ 新建分组</button>
    </div>
    <div class="group-list">
      <div v-for="group in groups" :key="group.id" class="group-card">
        <div class="group-info">
          <span class="group-name">{{ group.name }}</span>
          <span class="group-count">{{ getGroupCount(group.id) }} 个待办</span>
        </div>
        <div class="group-actions">
          <button class="btn btn-secondary btn-sm" @click="edit(group)">编辑</button>
          <button class="btn btn-danger btn-sm" @click="remove(group)">删除</button>
        </div>
      </div>
      <div v-if="groups.length === 0" class="empty">暂无分组</div>
    </div>

    <!-- 新建/编辑对话框 -->
    <div v-if="showCreate || editing" class="modal-overlay" @click.self="close">
      <div class="modal">
        <h3>{{ editing ? '编辑分组' : '新建分组' }}</h3>
        <input v-model.trim="form.name" class="form-input" placeholder="分组名称" />
        <textarea v-model.trim="form.description" class="form-input" placeholder="分组描述（可选）" rows="3"></textarea>
        <div class="modal-actions">
          <button class="btn btn-secondary" @click="close">取消</button>
          <button class="btn btn-primary" @click="save" :disabled="!form.name">保存</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, inject } from 'vue'
import api from '../api.js'

const showToast = inject('showToast')
const groups = ref([])
const todos = ref([])
const showCreate = ref(false)
const editing = ref(null)
const form = ref({ name: '', description: '' })

async function loadData() {
  const rg = await api.listGroups()
  if (rg.code === 0) groups.value = rg.data
  const rt = await api.list({ status: 'all', per_page: 1000 })
  if (rt.code === 0) todos.value = rt.data.items
}

function getGroupCount(groupId) {
  return todos.value.filter(t => t.group_id === groupId).length
}

function edit(group) {
  editing.value = group
  form.value = { name: group.name, description: group.description || '' }
}

function remove(group) {
  if (!confirm(`确认删除分组 "${group.name}"？`)) return
  api.deleteGroup(group.id).then(r => {
    if (r.code === 0) { showToast('已删除'); loadData() }
    else showToast(r.msg || '删除失败', 'error')
  })
}

async function save() {
  if (!form.value.name) return
  if (editing.value) {
    const r = await api.updateGroup(editing.value.id, form.value)
    if (r.code === 0) { showToast('已更新'); close(); loadData() }
    else showToast(r.msg || '更新失败', 'error')
  } else {
    const r = await api.createGroup(form.value)
    if (r.code === 0) { showToast('已创建'); close(); loadData() }
    else showToast(r.msg || '创建失败', 'error')
  }
}

function close() {
  showCreate.value = false
  editing.value = null
  form.value = { name: '', description: '' }
}

onMounted(loadData)
</script>

<style scoped>
.group-manager { padding: 24px; max-width: 700px; margin: 0 auto; }
.manager-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; }
.group-list { display: flex; flex-direction: column; gap: 12px; }
.group-card { background: var(--bg-elev); padding: 16px 20px; border-radius: var(--radius); display: flex; justify-content: space-between; align-items: center; }
.group-info { display: flex; flex-direction: column; gap: 4px; }
.group-name { font-weight: 600; font-size: 16px; }
.group-count { font-size: 13px; color: var(--text-muted); }
.group-actions { display: flex; gap: 8px; }
.empty { text-align: center; color: var(--text-muted); padding: 40px; }
.modal-overlay { position: fixed; inset: 0; background: rgba(0,0,0,0.5); display: flex; align-items: center; justify-content: center; z-index: 100; }
.modal { background: var(--bg-elev); padding: 24px; border-radius: var(--radius); width: 400px; }
.modal h3 { margin-bottom: 16px; }
.modal .form-input { margin-bottom: 12px; }
.modal-actions { display: flex; gap: 8px; justify-content: flex-end; margin-top: 16px; }
</style>
