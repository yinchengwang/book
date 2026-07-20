<template>
  <div class="plan-manage-page">
    <div class="panel">
      <div class="panel-left">
        <h3>任务系统</h3>
        <div class="system-list">
          <div v-for="ts in taskSystems" :key="ts.id"
               :class="['system-item', { active: selectedTs === ts.id }]"
               @click="selectedTs = ts.id">
            <span class="system-color" :style="{ background: ts.color }"></span>
            {{ ts.name }}
          </div>
        </div>
        <div class="panel-actions">
          <button class="btn btn-sm" @click="showCreateTs = true">+ 新建系统</button>
        </div>
        <!-- 新建任务系统弹窗 -->
        <div class="modal-overlay" v-if="showCreateTs" @click.self="showCreateTs = false">
          <div class="modal">
            <h4>新建任务系统</h4>
            <input v-model="newTsName" placeholder="系统名称" class="form-input" />
            <div class="modal-actions">
              <button class="btn btn-secondary" @click="showCreateTs = false">取消</button>
              <button class="btn btn-primary" @click="handleCreateTs">创建</button>
            </div>
          </div>
        </div>
      </div>
      <div class="panel-right">
        <h3>学习计划</h3>
        <div class="plan-list">
          <div v-for="plan in plans" :key="plan.id" class="plan-card">
            <div class="plan-header">
              <span class="plan-name">{{ plan.name }}</span>
              <span class="plan-date">{{ formatDate(plan.start_date) }} → {{ formatDate(plan.end_date) }}</span>
            </div>
            <p class="plan-desc">{{ plan.description }}</p>
            <div class="plan-actions">
              <button class="btn btn-sm" @click="handleExpand(plan.id)">展开为 Todo</button>
            </div>
          </div>
        </div>
        <div class="panel-actions">
          <button class="btn btn-primary" @click="handleImport">导入预置计划</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import api from '../api.js'

const selectedTs = ref(null)
const taskSystems = ref([])
const plans = ref([])
const showCreateTs = ref(false)
const newTsName = ref('')

async function load() {
  const r1 = await api.listTaskSystems()
  if (r1.code === 0) {
    taskSystems.value = r1.data
    if (!selectedTs.value && r1.data.length > 0) selectedTs.value = r1.data[0].id
  }
  const r2 = await api.listPlans()
  if (r2.code === 0) plans.value = r2.data
}

async function handleCreateTs() {
  if (!newTsName.value.trim()) return
  const res = await api.createTaskSystem({ name: newTsName.value, color: '#4A90D9' })
  if (res.code === 0) {
    showCreateTs.value = false
    newTsName.value = ''
    load()
  }
}

async function handleImport() {
  if (!selectedTs.value) { alert('请先选择一个任务系统'); return }
  const startDate = Math.floor(Date.now() / 1000)
  const res = await api.importTemplate({ template_id: 1, start_date: startDate, task_system_id: selectedTs.value })
  if (res.code === 0) {
    alert('计划导入成功，可在"学习计划"列表中查看')
    load()
  }
}

async function handleExpand(planId) {
  if (!selectedTs.value) { alert('请先选择一个任务系统'); return }
  const res = await api.expandPlan(planId, { task_system_id: selectedTs.value })
  if (res.code === 0) {
    alert(`计划已展开为任务，共创建 ${res.data?.created || 0} 个任务`)
  }
}

function formatDate(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getFullYear()}-${d.getMonth() + 1}-${d.getDate()}`
}

onMounted(load)
</script>

<style scoped>
.plan-manage-page { padding: 16px; background: #F3F3F3; min-height: 100vh; }
.plan-manage-page h3 { margin: 0 0 16px 0; font-size: 1.1em; }
.panel { display: flex; gap: 24px; background: #fff; border-radius: 8px; padding: 20px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.panel-left { width: 240px; border-right: 1px solid #eee; padding-right: 16px; }
.panel-right { flex: 1; }
.system-list { margin-bottom: 16px; }
.system-item { padding: 8px 12px; cursor: pointer; display: flex; align-items: center; gap: 8px; border-radius: 4px; }
.system-item:hover { background: #f0f0f0; }
.system-item.active { background: #e3f2fd; }
.system-color { width: 12px; height: 12px; border-radius: 50%; display: inline-block; }
.panel-actions { margin-top: 12px; }
.plan-list { margin-bottom: 16px; }
.plan-card { background: #fafafa; border: 1px solid #eee; border-radius: 8px; padding: 16px; margin-bottom: 12px; }
.plan-header { display: flex; justify-content: space-between; margin-bottom: 8px; }
.plan-name { font-weight: bold; }
.plan-date { color: #999; font-size: 0.85em; }
.plan-desc { white-space: pre-wrap; color: #666; font-size: 0.9em; margin-bottom: 12px; max-height: 80px; overflow-y: auto; }
.plan-actions { display: flex; gap: 8px; }
.btn { padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
.btn-sm { padding: 6px 12px; font-size: 13px; }
.btn-primary { background: #0078D4; color: #fff; }
.btn-primary:hover { background: #106EBE; }
.btn-secondary { background: #f0f0f0; color: #333; }
.btn-secondary:hover { background: #e0e0e0; }
.form-input { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; margin-bottom: 12px; }
.modal-overlay { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.4); display: flex; align-items: center; justify-content: center; z-index: 1000; }
.modal { background: #fff; border-radius: 8px; padding: 24px; min-width: 360px; }
.modal h4 { margin: 0 0 16px 0; }
.modal-actions { display: flex; gap: 8px; justify-content: flex-end; }
</style>