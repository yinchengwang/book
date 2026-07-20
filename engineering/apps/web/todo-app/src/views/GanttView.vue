<template>
  <div class="gantt-view">
    <ViewSelector @view-changed="onViewChanged" />

    <div class="gantt-toolbar">
      <button class="btn-text" @click="prevPeriod">◀</button>
      <span class="period-label">{{ periodLabel }}</span>
      <button class="btn-text" @click="nextPeriod">▶</button>
      <button class="btn-text" @click="goToday">今天</button>
    </div>

    <div class="gantt-container">
      <div class="gantt-sidebar">
        <div class="sidebar-header">任务</div>
        <div v-for="todo in visibleTodos" :key="todo.id" class="sidebar-row" @click="select(todo)">
          <span class="task-name">{{ todo.title }}</span>
        </div>
      </div>

      <div class="gantt-timeline" ref="timelineRef">
        <!-- 时间轴头部 -->
        <div class="timeline-header">
          <div v-for="col in timelineCols" :key="col.key" class="timeline-col-header" :style="{ width: colWidth + 'px' }">
            {{ col.label }}
          </div>
        </div>

        <!-- 时间轴主体 -->
        <div class="timeline-body">
          <div v-for="(todo, idx) in visibleTodos" :key="todo.id" class="timeline-row">
            <div v-for="col in timelineCols" :key="col.key" class="timeline-cell" :style="{ width: colWidth + 'px' }">
              <div
                v-if="isInCell(todo, col)"
                class="gantt-bar"
                :style="getBarStyle(todo, col)"
                @click="select(todo)"
              >
                {{ todo.title }}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <DetailPanel v-if="current" :todo="current" @updated="loadData" @close="current = null" />
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import api from '../api.js'
import ViewSelector from '../components/ViewSelector.vue'

const showToast = { value: { msg: '' }, inject: () => showToast }

const todos = ref([])
const current = ref(null)
const currentView = ref(null)
const colWidth = ref(40)
const viewStart = ref(new Date())

// 时间段
const periodLabel = computed(() => {
  const d = new Date(viewStart.value)
  return `${d.getFullYear()}年${d.getMonth() + 1}月`
})

const timelineCols = computed(() => {
  const cols = []
  const start = new Date(viewStart.value)
  start.setDate(1)
  const daysInMonth = new Date(start.getFullYear(), start.getMonth() + 1, 0).getDate()

  for (let i = 0; i < daysInMonth; i++) {
    const d = new Date(start)
    d.setDate(i + 1)
    cols.push({
      key: d.toISOString().slice(0, 10),
      label: d.getDate(),
      date: d
    })
  }
  return cols
})

const visibleTodos = computed(() => {
  return todos.value.filter(t => t.status !== 'archived' && t.due_date > 0)
})

function onViewChanged(view) {
  currentView.value = view
  loadData()
}

async function loadData() {
  const r = await api.list({ status: 'all', per_page: 1000 })
  if (r.code === 0) todos.value = r.data.items
}

async function select(todo) {
  const r = await api.get(todo.id)
  if (r.code === 0) current.value = r.data
}

function prevPeriod() {
  const d = new Date(viewStart.value)
  d.setMonth(d.getMonth() - 1)
  viewStart.value = d
}

function nextPeriod() {
  const d = new Date(viewStart.value)
  d.setMonth(d.getMonth() + 1)
  viewStart.value = d
}

function goToday() {
  viewStart.value = new Date()
}

function isInCell(todo, col) {
  if (!todo.due_date) return false
  const due = new Date(todo.due_date * 1000)
  const cellDate = col.date
  return due.toDateString() === cellDate.toDateString()
}

function getBarStyle(todo, col) {
  const due = new Date(todo.due_date * 1000)
  const cellDate = col.date
  const isToday = due.toDateString() === new Date().toDateString()
  const isPast = due < new Date()

  return {
    backgroundColor: isPast ? '#e74c3c' : isToday ? '#f39c12' : '#3498db',
    width: '36px',
    left: '2px'
  }
}

onMounted(loadData)
</script>

<style scoped>
.gantt-view { height: 100vh; display: flex; flex-direction: column; overflow: hidden; }
.gantt-toolbar {
  display: flex; align-items: center; gap: 12px;
  padding: 8px 24px; background: var(--card-bg, #fff);
  border-bottom: 1px solid var(--border-color, #e0e0e0);
}
.period-label { font-weight: 600; min-width: 100px; text-align: center; }
.gantt-container { display: flex; flex: 1; overflow: hidden; }
.gantt-sidebar {
  width: 200px; flex-shrink: 0;
  border-right: 1px solid var(--border-color, #e0e0e0);
  background: var(--bg-elev, #f8f9fa);
}
.sidebar-header {
  height: 40px; display: flex; align-items: center; padding: 0 12px;
  font-weight: 600; background: var(--header-bg, #f0f4f8);
  border-bottom: 1px solid var(--border-color, #e0e0e0);
}
.sidebar-row {
  height: 36px; display: flex; align-items: center; padding: 0 12px;
  border-bottom: 1px solid var(--border-color, #e0e0e0);
  cursor: pointer;
}
.sidebar-row:hover { background: var(--hover-bg, #f0f0f0); }
.task-name {
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  font-size: 13px;
}
.gantt-timeline { flex: 1; overflow: auto; }
.timeline-header {
  display: flex; height: 40px;
  background: var(--header-bg, #f0f4f8);
  border-bottom: 1px solid var(--border-color, #e0e0e0);
  position: sticky; top: 0; z-index: 2;
}
.timeline-col-header {
  display: flex; align-items: center; justify-content: center;
  font-size: 11px; color: #666;
  border-right: 1px solid var(--border-color, #e0e0e0);
}
.timeline-body { position: relative; }
.timeline-row { display: flex; height: 36px; border-bottom: 1px solid var(--border-color, #e0e0e0); }
.timeline-cell {
  position: relative;
  border-right: 1px solid var(--border-color, #e0e0e0);
}
.gantt-bar {
  position: absolute; top: 4px; height: 28px;
  border-radius: 4px; color: white; font-size: 10px;
  display: flex; align-items: center; justify-content: center;
  cursor: pointer; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  box-shadow: 0 1px 3px rgba(0,0,0,0.2);
}
.gantt-bar:hover { opacity: 0.9; }
</style>
