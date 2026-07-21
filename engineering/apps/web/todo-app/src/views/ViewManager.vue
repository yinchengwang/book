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
            <div class="config-tabs">
              <button :class="['config-tab', { active: configTab === 'columns' }]" @click="configTab = 'columns'">列</button>
              <button :class="['config-tab', { active: configTab === 'filters' }]" @click="configTab = 'filters'">筛选</button>
              <button :class="['config-tab', { active: configTab === 'sort' }]" @click="configTab = 'sort'">排序</button>
            </div>
            <div v-show="configTab === 'columns'" class="config-panel">
              <div v-for="f in allFields" :key="f.id" class="field-row">
                <label class="checkbox-label">
                  <input type="checkbox" :checked="visibleFields.includes(f.id)" @change="toggleField(f.id)" />
                  {{ f.name }}
                </label>
              </div>
            </div>
            <div v-show="configTab === 'filters'" class="config-panel">
              <FilterBuilder :fields="allFields" v-model="configData.filters" />
            </div>
            <div v-show="configTab === 'sort'" class="config-panel">
              <SortBuilder :fields="allFields" v-model="configData.sort" />
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
import FilterBuilder from '../components/FilterBuilder.vue'
import SortBuilder from '../components/SortBuilder.vue'

const showToast = inject('showToast')
const views = ref([])
const showCreate = ref(false)
const editingView = ref(null)
const form = ref({ name: '', type: 'table' })
const configGroupField = ref('3')
const configDateField = ref('5')
const configTab = ref('columns')
const allFields = ref([])
const configData = ref({ filters: [], sort: [], visible_fields: [1, 3, 4, 5] })
const visibleFields = ref([1, 3, 4, 5])

async function loadViews() {
  const res = await api.listViews()
  if (res.code === 0) {
    views.value = res.data
  }
}

async function loadFields() {
  const res = await api.listFields()
  if (res.code === 0) allFields.value = res.data
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
  const cfg = {
    visible_fields: visibleFields.value,
    filters: configData.value.filters,
    sort: configData.value.sort,
  }
  if (form.value.type === 'board') {
    const fid = parseInt(configGroupField.value) || 3
    cfg.group_field = String(fid)
    cfg.card_fields = [1, 5]
    cfg.card_cover_field = null
  } else if (form.value.type === 'calendar') {
    const fid = parseInt(configDateField.value) || 5
    cfg.date_field = String(fid)
    cfg.show_fields = visibleFields.value
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
    /* 编辑已有视图时，额外更新 config 中的 filter/sort/visible_fields */
    if (editingView.value) {
      const cfg = {
        visible_fields: visibleFields.value,
        filters: configData.value.filters,
        sort: configData.value.sort,
      }
      await api.updateViewConfig(editingView.value.id, cfg)
    }
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
  visibleFields.value = cfg.visible_fields || [1, 3, 4, 5]
  configData.value = {
    filters: cfg.filters || [],
    sort: cfg.sort || [],
    visible_fields: visibleFields.value,
  }
}

function toggleField(id) {
  const idx = visibleFields.value.indexOf(id)
  if (idx >= 0) {
    visibleFields.value.splice(idx, 1)
  } else {
    visibleFields.value.push(id)
  }
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
  configTab.value = 'columns'
  configData.value = { filters: [], sort: [], visible_fields: [1, 3, 4, 5] }
  visibleFields.value = [1, 3, 4, 5]
}

onMounted(() => {
  loadViews()
  loadFields()
})
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
.config-tabs {
  display: flex;
  gap: 4px;
  margin-bottom: 12px;
  border-bottom: 1px solid var(--border-color, #e0e0e0);
  padding-bottom: 4px;
}
.config-tab {
  padding: 4px 12px;
  border: none;
  background: none;
  cursor: pointer;
  font-size: 13px;
  color: #888;
  border-radius: 4px 4px 0 0;
}
.config-tab.active {
  color: var(--primary-color, #4A90D9);
  background: rgba(74, 144, 217, 0.08);
  font-weight: 500;
}
.config-panel {
  max-height: 300px;
  overflow-y: auto;
}
.field-row {
  padding: 4px 0;
}
.checkbox-label {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 13px;
  cursor: pointer;
}
</style>
