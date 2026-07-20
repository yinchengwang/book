<template>
  <div class="calendar-page">
    <div class="calendar-header">
      <button @click="prevMonth">◀</button>
      <h2>{{ currentYear }} 年 {{ currentMonth + 1 }} 月</h2>
      <button @click="nextMonth">▶</button>
      <div class="view-toggle">
        <button :class="{ active: view === 'month' }" @click="view = 'month'">月</button>
        <button :class="{ active: view === 'week' }" @click="view = 'week'">周</button>
        <button :class="{ active: view === 'day' }" @click="view = 'day'">日</button>
      </div>
    </div>
    <div v-if="view === 'month'" class="month-view">
      <div class="weekday-header">
        <div v-for="d in ['周一', '周二', '周三', '周四', '周五', '周六', '周日']" :key="d">{{ d }}</div>
      </div>
      <div class="days-grid">
        <div v-for="(day, idx) in monthDays" :key="idx"
             :class="['day-cell', { today: day.isToday, 'other-month': !day.isCurrentMonth, 'has-task': day.tasks.length > 0, 'weekend': day.isWeekend }]"
             @click="selectDay(day.date)">
          <span class="day-number">{{ day.dayNum }}</span>
          <div class="day-tasks">
            <div v-for="task in day.tasks.slice(0, 3)" :key="task.id" class="mini-task"
                 :style="{ borderLeftColor: priorityColor(task.priority) }">
              ☐ {{ task.title }}
            </div>
            <div v-if="day.tasks.length > 3" class="more-tasks">+{{ day.tasks.length - 3 }} 更多</div>
          </div>
        </div>
      </div>
    </div>
    <div v-else-if="view === 'week'" class="week-view">
      <div class="weekday-header">
        <div v-for="d in ['周一', '周二', '周三', '周四', '周五', '周六', '周日']" :key="d">{{ d }}</div>
      </div>
      <div class="week-grid">
        <div v-for="(day, idx) in weekDays" :key="idx" class="week-day-cell"
             :class="{ today: day.isToday, weekend: day.isWeekend }">
          <div class="week-day-header">
            <span class="week-day-num">{{ day.dayNum }}</span>
            <span class="week-day-name">{{ ['日','一','二','三','四','五','六'][new Date(day.date).getDay()] }}</span>
          </div>
          <div class="week-tasks">
            <div v-for="task in day.tasks" :key="task.id" class="mini-task"
                 :style="{ borderLeftColor: priorityColor(task.priority) }">
              ☐ {{ task.title }}
            </div>
          </div>
        </div>
      </div>
    </div>
    <div v-else class="day-view">
      <div class="day-header">
        <h3>{{ formatDateLong(selectedDate) }}</h3>
      </div>
      <div class="day-tasks-list">
        <div v-for="task in dayTasks" :key="task.id" class="day-task-item"
             :style="{ borderLeftColor: priorityColor(task.priority) }">
          <div class="task-priority-dot" :style="{ background: priorityColor(task.priority) }"></div>
          <div class="task-content">
            <div class="task-title">{{ task.title }}</div>
            <div class="task-meta" v-if="task.description">{{ task.description }}</div>
          </div>
        </div>
        <div v-if="dayTasks.length === 0" class="empty-tasks">今天没有任务</div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import api from '../api.js'

const view = ref('month')
const currentDate = ref(new Date())
const tasksByDate = ref({})
const selectedDate = ref(new Date())

const currentYear = computed(() => currentDate.value.getFullYear())
const currentMonth = computed(() => currentDate.value.getMonth())

async function loadMonth() {
  const ts = Math.floor(currentDate.value.getTime() / 1000)
  const res = await api.calendarMonth(ts, -1)
  if (res.code === 0 && res.data) {
    tasksByDate.value = {}
    for (const task of (res.data.tasks || [])) {
      const d = new Date(task.due_date * 1000)
      const dateKey = `${d.getFullYear()}-${d.getMonth()}-${d.getDate()}`
      if (!tasksByDate.value[dateKey]) tasksByDate.value[dateKey] = []
      tasksByDate.value[dateKey].push(task)
    }
  }
}

const monthDays = computed(() => {
  const year = currentYear.value
  const month = currentMonth.value
  const firstDay = new Date(year, month, 1)
  let startDow = firstDay.getDay()
  if (startDow === 0) startDow = 7
  const days = []
  const prevMonthDays = new Date(year, month, 0).getDate()
  for (let i = startDow - 1; i > 0; i--) {
    days.push({ dayNum: prevMonthDays - i + 1, isCurrentMonth: false, isToday: false, isWeekend: false, tasks: [], date: null })
  }
  const totalDays = new Date(year, month + 1, 0).getDate()
  const today = new Date()
  for (let d = 1; d <= totalDays; d++) {
    const date = new Date(year, month, d)
    const dateKey = `${date.getFullYear()}-${date.getMonth()}-${date.getDate()}`
    days.push({
      dayNum: d,
      isCurrentMonth: true,
      isToday: date.toDateString() === today.toDateString(),
      isWeekend: date.getDay() === 0 || date.getDay() === 6,
      tasks: tasksByDate.value[dateKey] || [],
      date: date
    })
  }
  return days
})

const weekDays = computed(() => {
  const ms = currentDate.value.getTime()
  const d = new Date(ms)
  let dow = d.getDay()
  if (dow === 0) dow = 7
  const monday = new Date(d.getTime() - (dow - 1) * 86400000)
  const days = []
  const today = new Date()
  for (let i = 0; i < 7; i++) {
    const date = new Date(monday.getTime() + i * 86400000)
    const dateKey = `${date.getFullYear()}-${date.getMonth()}-${date.getDate()}`
    days.push({
      dayNum: date.getDate(),
      isToday: date.toDateString() === today.toDateString(),
      isWeekend: i >= 5,
      tasks: tasksByDate.value[dateKey] || [],
      date: date
    })
  }
  return days
})

const dayTasks = computed(() => {
  const d = selectedDate.value
  if (!d) return []
  const dateKey = `${d.getFullYear()}-${d.getMonth()}-${d.getDate()}`
  return tasksByDate.value[dateKey] || []
})

function prevMonth() { currentDate.value = new Date(currentYear.value, currentMonth.value - 1, 1) }
function nextMonth() { currentDate.value = new Date(currentYear.value, currentMonth.value + 1, 1) }
function selectDay(date) {
  if (date) { selectedDate.value = date; view.value = 'day' }
}
function priorityColor(p) {
  return ['#E53935', '#FB8C00', '#FFB300', '#42A5F5', '#9E9E9E'][p] || '#9E9E9E'
}
function formatDateLong(date) {
  if (!date) return ''
  return `${date.getFullYear()}年${date.getMonth() + 1}月${date.getDate()}日`
}

onMounted(loadMonth)
watch(currentDate, loadMonth)
</script>

<style scoped>
.calendar-page { padding: 16px; background: #F3F3F3; min-height: 100vh; }
.calendar-header { display: flex; align-items: center; gap: 12px; margin-bottom: 16px; background: #fff; padding: 12px 16px; border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.08); }
.calendar-header h2 { flex: 1; font-size: 1.3em; margin: 0; }
.calendar-header button { padding: 6px 12px; border: 1px solid #ddd; background: #fff; cursor: pointer; border-radius: 4px; font-size: 14px; }
.calendar-header button:hover { background: #f0f0f0; }
.view-toggle { display: flex; gap: 0; }
.view-toggle button { padding: 6px 14px; border: 1px solid #ddd; background: #fff; cursor: pointer; }
.view-toggle button:first-child { border-radius: 4px 0 0 4px; }
.view-toggle button:last-child { border-radius: 0 4px 4px 0; }
.view-toggle button.active { background: #0078D4; color: #fff; border-color: #0078D4; }
.weekday-header { display: grid; grid-template-columns: repeat(7, 1fr); text-align: center; font-weight: 600; color: #666; padding: 8px 0; font-size: 0.85em; }
.days-grid { display: grid; grid-template-columns: repeat(7, 1fr); gap: 1px; background: #e0e0e0; border: 1px solid #e0e0e0; border-radius: 4px; overflow: hidden; }
.day-cell { min-height: 90px; background: #fff; padding: 4px; cursor: pointer; }
.day-cell:hover { background: #f5f5f5; }
.day-cell.today .day-number { background: #0078D4; color: #fff; border-radius: 50%; width: 24px; height: 24px; display: inline-flex; align-items: center; justify-content: center; }
.day-cell.other-month { background: #fafafa; }
.day-cell.other-month .day-number { color: #ccc; }
.day-cell.weekend { background: #f8f8f8; }
.day-number { font-size: 0.85em; font-weight: 500; margin-bottom: 4px; display: inline-block; text-align: center; min-width: 24px; }
.mini-task { font-size: 0.75em; padding: 2px 4px; border-left: 3px solid; margin-bottom: 2px; cursor: pointer; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; border-radius: 0 2px 2px 0; }
.mini-task:hover { background: #f0f0f0; }
.more-tasks { font-size: 0.75em; color: #0078D4; cursor: pointer; font-weight: 500; }
.week-view { background: #fff; border-radius: 8px; padding: 12px; box-shadow: 0 1px 3px rgba(0,0,0,0.08); }
.week-grid { display: grid; grid-template-columns: repeat(7, 1fr); gap: 4px; }
.week-day-cell { min-height: 200px; border: 1px solid #eee; border-radius: 4px; padding: 8px; }
.week-day-cell.today { border-color: #0078D4; background: #f0f7ff; }
.week-day-cell.weekend { background: #fafafa; }
.week-day-header { text-align: center; margin-bottom: 8px; }
.week-day-num { font-size: 1.1em; font-weight: 600; }
.week-day-name { font-size: 0.75em; color: #999; margin-left: 4px; }
.week-tasks { display: flex; flex-direction: column; gap: 3px; }
.day-view { background: #fff; border-radius: 8px; padding: 16px; box-shadow: 0 1px 3px rgba(0,0,0,0.08); }
.day-header { margin-bottom: 16px; }
.day-header h3 { margin: 0; font-size: 1.1em; }
.day-tasks-list { display: flex; flex-direction: column; gap: 8px; }
.day-task-item { display: flex; align-items: flex-start; gap: 8px; padding: 10px 12px; border-left: 3px solid; background: #fff; border-radius: 4px; box-shadow: 0 1px 2px rgba(0,0,0,0.05); }
.task-priority-dot { width: 8px; height: 8px; border-radius: 50%; margin-top: 4px; flex-shrink: 0; }
.task-content { flex: 1; }
.task-title { font-size: 0.9em; font-weight: 500; }
.task-meta { font-size: 0.8em; color: #999; margin-top: 2px; }
.empty-tasks { text-align: center; color: #999; padding: 32px 0; font-size: 0.9em; }
</style>