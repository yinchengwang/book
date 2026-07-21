<template>
  <div class="gantt-view">
    <ViewSelector @view-changed="onViewChanged" />

    <div class="gantt-toolbar">
      <button class="btn-text" @click="prevPeriod">◀</button>
      <span class="period-label">{{ periodLabel }}</span>
      <button class="btn-text" @click="nextPeriod">▶</button>
      <button class="btn-text" @click="goToday">今天</button>
      <span class="zoom-controls">
        <button class="btn-text" @click="zoomOut">−</button>
        <span class="zoom-label">{{ colWidth }}px</span>
        <button class="btn-text" @click="zoomIn">+</button>
      </span>
    </div>

    <div class="gantt-container">
      <div class="gantt-sidebar">
        <div class="sidebar-header">任务</div>
        <div v-for="todo in visibleTodos" :key="todo.id" class="sidebar-row" @click="select(todo)">
          <span class="task-name">{{ todo.title }}</span>
        </div>
      </div>

      <div class="gantt-timeline" ref="timelineRef" @scroll="onScroll">
        <!-- 时间轴头部 -->
        <div class="timeline-header">
          <div v-for="col in timelineCols" :key="col.key" class="timeline-col-header"
               :class="{ 'weekend-col': col.isWeekend, 'today-col': col.isToday }"
               :style="{ width: colWidth + 'px' }">
            <div class="col-label">{{ col.label }}</div>
            <div class="col-weekday">{{ col.weekday }}</div>
          </div>
        </div>

        <!-- 时间轴主体 -->
        <div class="timeline-body">
          <div v-for="(todo, idx) in visibleTodos" :key="todo.id" class="timeline-row">
            <div v-for="col in timelineCols" :key="col.key" class="timeline-cell"
                 :class="{ 'weekend-col': col.isWeekend, 'today-col': col.isToday }"
                 :style="{ width: colWidth + 'px' }"
                 @dragover.prevent @drop="onBarDrop($event, todo, col)">
              <!-- 多日条：起止日期之间跨多列 -->
              <div
                v-if="isBarStart(todo, col)"
                class="gantt-bar"
                :class="getBarClass(todo)"
                :style="getBarStyle(todo, col)"
                draggable="true"
                @dragstart="onBarDragStart($event, todo)"
                @click="select(todo)"
              >
                <span class="bar-title">{{ todo.title }}</span>
                <span class="bar-progress" v-if="todo.progress > 0">{{ todo.progress }}%</span>
              </div>
              <!-- 拖拽调整手柄（开始/结束） -->
              <div
                v-if="isBarStart(todo, col)"
                class="bar-resize bar-resize-left"
                @mousedown.prevent="startResize($event, todo, 'left')"
              ></div>
              <div
                v-if="isBarEnd(todo, col)"
                class="bar-resize bar-resize-right"
                @mousedown.prevent="startResize($event, todo, 'right')"
              ></div>
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
import DetailPanel from '../components/DetailPanel.vue'

const todos = ref([])
const current = ref(null)
const currentView = ref(null)
const colWidth = ref(36)
const viewStart = ref(new Date())
const draggedTask = ref(null)
const resizing = ref(null)  // { todo, side: 'left'|'right' }

// 时间段
const periodLabel = computed(() => {
  const d = new Date(viewStart.value)
  return `${d.getFullYear()}年${d.getMonth() + 1}月`
})

const WEEKDAY_NAMES = ['日', '一', '二', '三', '四', '五', '六']

const timelineCols = computed(() => {
  const cols = []
  const start = new Date(viewStart.value)
  start.setDate(1)
  const daysInMonth = new Date(start.getFullYear(), start.getMonth() + 1, 0).getDate()
  const today = new Date()

  for (let i = 0; i < daysInMonth; i++) {
    const d = new Date(start)
    d.setDate(i + 1)
    const dow = d.getDay()
    cols.push({
      key: d.toISOString().slice(0, 10),
      label: d.getDate(),
      weekday: WEEKDAY_NAMES[dow],
      date: d,
      isWeekend: dow === 0 || dow === 6,
      isToday: d.toDateString() === today.toDateString()
    })
  }
  return cols
})

const visibleTodos = computed(() => {
  return todos.value.filter(t => t.status !== 'archived' && t.due_date > 0)
})

const progressMap = ref({})

function onViewChanged(view) {
  currentView.value = view
  loadData()
}

async function loadData() {
  const r = await api.list({ status: 'all', per_page: 1000, view_id: currentView.value?.id || 0 })
  if (r.code === 0) {
    todos.value = r.data.items
    // 加载进度数据
    for (const t of todos.value) {
      if (t.due_date > 0) loadProgress(t)
    }
  }
}

async function loadProgress(todo) {
  // 从 checklist 估算进度
  const r = await api.listChecklist(todo.id)
  if (r.code === 0 && r.data && r.data.length > 0) {
    const done = r.data.filter(i => i.done).length
    progressMap.value[todo.id] = Math.round((done / r.data.length) * 100)
  } else {
    progressMap.value[todo.id] = 0
  }
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

function zoomIn() {
  colWidth.value = Math.min(colWidth.value + 8, 80)
}

function zoomOut() {
  colWidth.value = Math.max(colWidth.value - 8, 24)
}

function onScroll() {
  // 同步滚动用
}

/* ===== 日期计算 ===== */
function getStartDate(todo) {
  if (todo.original_date > 0) return new Date(todo.original_date * 1000)
  // 如果没有 original_date，从 due_date 往前推 1 天
  if (todo.due_date > 0) {
    const d = new Date(todo.due_date * 1000)
    d.setDate(d.getDate() - 1)
    return d
  }
  return null
}

function getEndDate(todo) {
  return todo.due_date > 0 ? new Date(todo.due_date * 1000) : null
}

function isInCell(todo, col) {
  const start = getStartDate(todo)
  const end = getEndDate(todo)
  if (!start || !end) return false
  const cellDate = col.date
  return cellDate >= start && cellDate <= end
}

function isBarStart(todo, col) {
  const start = getStartDate(todo)
  if (!start) return false
  return col.date.toDateString() === start.toDateString()
}

function isBarEnd(todo, col) {
  const end = getEndDate(todo)
  if (!end) return false
  return col.date.toDateString() === end.toDateString()
}

function getBarStyle(todo, col) {
  const start = getStartDate(todo)
  const end = getEndDate(todo)
  if (!start || !end) return {}

  // 计算跨越的天数
  const diffDays = Math.ceil((end - start) / 86400000) + 1
  const width = diffDays * colWidth.value - 4
  const progress = progressMap.value[todo.id] || 0

  return {
    width: width + 'px',
    left: '2px',
    background: progress > 0
      ? `linear-gradient(90deg, ${getBarColor(todo)} ${progress}%, rgba(0,0,0,0.1) ${progress}%)`
      : getBarColor(todo)
  }
}

function getBarClass(todo) {
  const end = getEndDate(todo)
  if (!end) return ''
  const isPast = end < new Date()
  return {
    'bar-past': isPast && todo.status !== 'closed',
    'bar-done': todo.status === 'closed',
    'bar-active': !isPast && todo.status !== 'closed'
  }
}

function getBarColor(todo) {
  if (todo.status === 'closed') return '#4caf50'
  const end = getEndDate(todo)
  if (end && end < new Date()) return '#e74c3c'
  const p = todo.priority
  return ['#e53935', '#fb8c00', '#ffb300', '#42a5f5', '#9e9e9e'][p] || '#9e9e9e'
}

/* ===== 拖拽移动 ===== */
function onBarDragStart(e, todo) {
  draggedTask.value = todo
  e.dataTransfer.effectAllowed = 'move'
}

async function onBarDrop(e, todo, col) {
  if (!draggedTask.value || !col) return
  const task = draggedTask.value
  draggedTask.value = null

  if (task.id === todo.id) return  // 拖到自身
  // 批量调整
  const ts = Math.floor(col.date.getTime() / 1000)
  const diff = task.due_date - task.original_date
  await api.update(task.id, {
    due_date: ts,
    original_date: ts - (diff > 0 ? diff : 86400)
  })
  loadData()
}

/* ===== 拖拽调整起止日期 ===== */
function startResize(e, todo, side) {
  resizing.value = { todo, side }
  document.addEventListener('mousemove', onResizeMove)
  document.addEventListener('mouseup', onResizeEnd)
}

function onResizeMove(e) {
  if (!resizing.value) return
  // 拖拽过程中高亮效果 — 简化实现，实际通过 mouseup 写入
}

async function onResizeEnd(e) {
  if (!resizing.value) return
  document.removeEventListener('mousemove', onResizeMove)
  document.removeEventListener('mouseup', onResizeEnd)

  const { todo, side } = resizing.value
  resizing.value = null

  // 获取鼠标位置对应的列索引
  const timeline = document.querySelector('.gantt-timeline')
  if (!timeline) return
  const rect = timeline.getBoundingClientRect()
  const scrollLeft = timeline.scrollLeft
  const relX = e.clientX - rect.left + scrollLeft
  const colIndex = Math.floor(relX / colWidth.value)
  if (colIndex < 0 || colIndex >= timelineCols.value.length) return

  const targetDate = timelineCols.value[colIndex].date
  const ts = Math.floor(targetDate.getTime() / 1000)

  if (side === 'left') {
    // 调整 original_date
    await api.update(todo.id, { original_date: ts })
  } else {
    // 调整 due_date
    await api.update(todo.id, { due_date: ts })
  }
  loadData()
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
.zoom-controls { margin-left: auto; display: flex; align-items: center; gap: 4px; }
.zoom-label { font-size: 12px; color: #888; min-width: 32px; text-align: center; }
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
  display: flex; flex-direction: column;
  align-items: center; justify-content: center;
  font-size: 11px; color: #666;
  border-right: 1px solid var(--border-color, #e0e0e0);
}
.col-label { font-weight: 700; }
.col-weekday { font-size: 9px; color: #999; }
.weekend-col { background: #fafafa; }
.today-col { background: #fff8e1; }
.timeline-body { position: relative; }
.timeline-row { display: flex; height: 36px; border-bottom: 1px solid var(--border-color, #e0e0e0); }
.timeline-cell {
  position: relative;
  border-right: 1px solid var(--border-color, #e0e0e0);
}
.gantt-bar {
  position: absolute; top: 4px; height: 28px;
  border-radius: 4px; color: white; font-size: 10px;
  display: flex; align-items: center; gap: 4px; padding: 0 6px;
  cursor: grab; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
  box-shadow: 0 1px 3px rgba(0,0,0,0.2); z-index: 1;
  transition: opacity 0.15s;
}
.gantt-bar:hover { opacity: 0.9; }
.bar-past { opacity: 0.7; }
.bar-done { opacity: 0.6; }
.bar-title { flex: 1; overflow: hidden; text-overflow: ellipsis; }
.bar-progress { font-size: 9px; background: rgba(0,0,0,0.2); padding: 0 4px; border-radius: 2px; }

/* 拖拽调整手柄 */
.bar-resize {
  position: absolute; top: 0; width: 6px; height: 100%;
  cursor: col-resize; z-index: 3;
}
.bar-resize-left { left: 0; }
.bar-resize-right { right: 0; }
.bar-resize:hover { background: rgba(0,0,0,0.15); }
</style>