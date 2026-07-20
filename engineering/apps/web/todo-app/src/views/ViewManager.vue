<template>
  <div class="view-manager">
    <div class="view-header">
      <h2>视图管理</h2>
      <button class="btn-primary btn-sm" @click="showCreate = true">+ 新建视图</button>
    </div>

    <!-- 视图列表 -->
    <div class="view-list">
      <div
        v-for="view in views"
        :key="view.id"
        :class="['view-item', { 'view-item--active': view.is_default }]"
      >
        <div class="view-icon">{{ viewIcon(view.type) }}</div>
        <div class="view-info">
          <div class="view-name">{{ view.name }}</div>
          <div class="view-type">{{ viewTypeLabel(view.type) }}</div>
        </div>
        <div class="view-actions">
          <button v-if="!view.is_default" class="btn-text" @click="setDefault(view.id)" title="设为默认">⭐</button>
          <button class="btn-text" @click="editView(view)" title="编辑">✏️</button>
          <button class="btn-text btn-danger" @click="deleteView(view.id)" title="删除">🗑️</button>
        </div>
      </div>
    </div>

    <!-- 创建/编辑弹窗 -->
    <div v-if="showCreate || editingView" class="modal-overlay" @click.self="closeModal">
      <div class="modal">
        <div class="modal-header">
          <h3>{{ editingView ? '编辑视图' : '新建视图' }}</h3>
          <button class="close-btn" @click="closeModal">×</button>
        </div>
        <div class="modal-body">
          <div class="form-group">
            <label>视图名称</label>
            <input v-model="form.name" class="form-input" placeholder="输入视图名称" />
          </div>
          <div class="form-group">
            <label>视图类型</label>
            <select v-model="form.type" class="form-input">
              <option value="table">📋 表格视图</option>
              <option value="board">📊 看板视图</option>
              <option value="calendar">📅 日历视图</option>
              <option value="gantt">📈 甘特图</option>
            </select>
          </div>
          <div class="form-group">
            <label>视图配置</label>
            <div class="config-hint">
              <template v-if="form.type === 'board'">
                <p>看板视图配置：</p>
                <input v-model="configGroupField" type="number" class="form-input config-input" placeholder="分组字段 ID (如: 3)" />
                <small>分组字段通常是"状态"字段 (ID: 3)</small>
              </template>
              <template v-else-if="form.type === 'calendar'">
                <p>日历视图配置：</p>
                <input v-model="configDateField" type="number" class="form-input config-input" placeholder="日期字段 ID (如: 5)" />
              </template>
              <template v-else-if="form.type === 'gantt'">
                <p>甘特图配置：</p>
                <input v-model="configDateField" type="number" class="form-input config-input" placeholder="开始日期字段 ID" />
              </template>
              <template v-else>
                <p>表格视图将显示所有字段，可自定义显示列</p>
              </template>
            </div>
          </div>
        </div>
        <div class="modal-footer">
          <button class="btn-secondary" @click="closeModal">取消</button>
          <button class="btn-primary" @click="saveView">{{ editingView ? '保存' : '创建' }}</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, inject } from 'vue'
import api from '../api.js'

const showToast = inject('showToast')
const views = ref([])
const showCreate = ref(false)
const editingView = ref(null)
const form = ref({ name: '', type: 'table' })
const configGroupField = ref('3')
const configDateField = ref('5')

async function loadViews() {
  const res = await api.listViews()
  if (res.code === 0) {
    views.value = res.data
  }
}

function viewIcon(type) {
  const icons = { table: '📋', board: '📊', calendar: '📅', gantt: '📈' }
  return icons[type] || '📋'
}

function viewTypeLabel(type) {
  const labels = { table: '表格视图', board: '看板视图', calendar: '日历视图', gantt: '甘特图' }
  return labels[type] || '表格视图'
}

function buildConfig() {
  const cfg = {}
  if (form.value.type === 'board') {
    const fid = parseInt(configGroupField.value) || 3
    cfg.group_field = String(fid)
    cfg.card_fields = [1, 5]
    cfg.card_cover_field = null
  } else if (form.value.type === 'calendar') {
    const fid = parseInt(configDateField.value) || 5
    cfg.date_field = String(fid)
    cfg.show_fields = [1, 3]
  } else if (form.value.type === 'gantt') {
    const fid = parseInt(configDateField.value) || 5
    cfg.start_field = String(fid)
    cfg.end_field = null
    cfg.bar_label_field = '1'
  }
  return JSON.stringify(cfg)
}

async function saveView() {
  if (!form.value.name.trim()) {
    showToast('请输入视图名称', 'error')
    return
  }
  const body = {
    name: form.value.name.trim(),
    type: form.value.type,
    config: buildConfig(),
  }
  const res = editingView.value
    ? await api.updateView(editingView.value.id, body)
    : await api.createView(body)
  if (res.code === 0) {
    showToast(editingView.value ? '视图已更新' : '视图已创建')
    closeModal()
    loadViews()
  } else {
    showToast(res.msg || '操作失败', 'error')
  }
}

async function setDefault(id) {
  const res = await api.setViewDefault(id)
  if (res.code === 0) {
    showToast('已设为默认视图')
    loadViews()
  } else {
    showToast(res.msg || '操作失败', 'error')
  }
}

function editView(view) {
  editingView.value = view
  form.value = { name: view.name, type: view.type }
  const cfg = view.config || {}
  configGroupField.value = cfg.group_field || '3'
  configDateField.value = cfg.date_field || '5'
}

async function deleteView(id) {
  if (!confirm('确定删除此视图？')) return
  const res = await api.deleteView(id)
  if (res.code === 0) {
    showToast('视图已删除')
    loadViews()
  } else {
    showToast(res.msg || '删除失败', 'error')
  }
}

function closeModal() {
  showCreate.value = false
  editingView.value = null
  form.value = { name: '', type: 'table' }
  configGroupField.value = '3'
  configDateField.value = '5'
}

onMounted(loadViews)
</script>

<style scoped>
.view-manager {
  padding: 20px;
}
.view-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
}
.view-header h2 {
  margin: 0;
  font-size: 18px;
}
.view-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}
.view-item {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  background: var(--card-bg, #fff);
  border: 1px solid var(--border-color, #e0e0e0);
  border-radius: 8px;
  transition: all 0.2s;
}
.view-item:hover {
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
}
.view-item--active {
  border-color: var(--primary-color, #4A90D9);
  background: rgba(74, 144, 217, 0.05);
}
.view-icon {
  font-size: 24px;
}
.view-info {
  flex: 1;
}
.view-name {
  font-weight: 500;
}
.view-type {
  font-size: 12px;
  color: #888;
}
.view-actions {
  display: flex;
  gap: 8px;
}
.btn-text {
  background: none;
  border: none;
  cursor: pointer;
  padding: 4px 8px;
  font-size: 16px;
  opacity: 0.6;
  transition: opacity 0.2s;
}
.btn-text:hover {
  opacity: 1;
}
.btn-danger:hover {
  color: #e74c3c;
}
.modal-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0,0,0,0.5);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
}
.modal {
  background: var(--card-bg, #fff);
  border-radius: 12px;
  width: 480px;
  max-width: 90vw;
  box-shadow: 0 8px 32px rgba(0,0,0,0.2);
}
.modal-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 20px;
  border-bottom: 1px solid var(--border-color, #e0e0e0);
}
.modal-header h3 { margin: 0; }
.modal-body {
  padding: 20px;
}
.modal-footer {
  display: flex;
  justify-content: flex-end;
  gap: 12px;
  padding: 16px 20px;
  border-top: 1px solid var(--border-color, #e0e0e0);
}
.form-group {
  margin-bottom: 16px;
}
.form-group label {
  display: block;
  margin-bottom: 6px;
  font-weight: 500;
}
.form-group small {
  display: block;
  margin-top: 4px;
  color: #888;
  font-size: 12px;
}
.config-input {
  margin-top: 8px;
}
</style>
