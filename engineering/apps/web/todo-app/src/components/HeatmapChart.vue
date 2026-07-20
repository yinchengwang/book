<template>
  <div class="heatmap">
    <h3>学习热力图</h3>
    <div class="heatmap-grid">
      <div class="month-labels">
        <div v-for="(m, i) in monthLabels" :key="i">{{ m }}</div>
      </div>
      <div class="weekday-labels">
        <div v-for="d in ['一', '三', '五']" :key="d">{{ d }}</div>
      </div>
      <div class="cells">
        <div v-for="(week, wi) in data.weeks" :key="wi" class="week-column">
          <div v-for="(day, di) in week.days" :key="di"
               :class="['cell', 'level-' + day.level]"
               :title="`${formatDate(day.date)}: ${day.completed_count} 个完成`">
          </div>
        </div>
      </div>
    </div>
    <div class="heatmap-legend">
      <span>少</span>
      <div class="cell level-0"></div>
      <div class="cell level-1"></div>
      <div class="cell level-2"></div>
      <div class="cell level-3"></div>
      <span>多</span>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({ data: Object })

const monthLabels = computed(() => {
  const labels = []
  if (!props.data?.weeks?.length) return labels
  let lastMonth = -1
  for (const week of props.data.weeks) {
    for (const day of week.days) {
      if (!day.date) continue
      const d = new Date(day.date * 1000)
      if (d.getMonth() !== lastMonth) {
        labels.push(`${d.getMonth() + 1}月`)
        lastMonth = d.getMonth()
        break
      }
    }
  }
  return labels
})

function formatDate(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getFullYear()}/${d.getMonth() + 1}/${d.getDate()}`
}
</script>

<style scoped>
.heatmap { margin-top: 24px; }
.heatmap h3 { margin-bottom: 12px; }
.heatmap-grid { display: flex; gap: 4px; overflow-x: auto; }
.month-labels { display: flex; flex-direction: column; justify-content: space-around; font-size: 0.75em; color: #666; gap: 8px; min-width: 28px; }
.weekday-labels { display: flex; flex-direction: column; justify-content: space-around; font-size: 0.75em; color: #666; padding: 0 4px; }
.cells { display: flex; gap: 2px; }
.week-column { display: flex; flex-direction: column; gap: 2px; }
.cell { width: 12px; height: 12px; border-radius: 2px; }
.level-0 { background: #ebedf0; }
.level-1 { background: #9be9a8; }
.level-2 { background: #40c463; }
.level-3 { background: #30a14e; }
.heatmap-legend { display: flex; align-items: center; gap: 4px; margin-top: 8px; font-size: 0.75em; color: #666; }
</style>