<template>
  <div class="stats-dfx-page">
    <h2>DFX 统计</h2>
    <div class="summary-cards">
      <div class="card"><div class="card-value">{{ stats.total_completed }}</div><div class="card-label">总完成</div></div>
      <div class="card success"><div class="card-value">{{ stats.on_time_completed }}</div><div class="card-label">按时完成</div></div>
      <div class="card info"><div class="card-value">{{ stats.early_completed }}</div><div class="card-label">提前完成</div></div>
      <div class="card warning"><div class="card-value">{{ stats.carried_over_count }}</div><div class="card-label">顺延次数</div></div>
      <div class="card danger"><div class="card-value">{{ stats.overdue_count }}</div><div class="card-label">逾期未完成</div></div>
    </div>
    <div class="stats-row">
      <div class="stat-block">
        <span class="stat-label">完成率</span>
        <span class="stat-value">{{ (stats.completion_rate * 100).toFixed(1) }}%</span>
      </div>
      <div class="stat-block">
        <span class="stat-label">连续完成</span>
        <span class="stat-value">{{ stats.streak_days }} 天</span>
      </div>
      <div class="stat-block">
        <span class="stat-label">计划健康度</span>
        <span class="stat-value">{{ stats.plan_health_score.toFixed(0) }} 分</span>
      </div>
      <div class="stat-block">
        <span class="stat-label">平均提前</span>
        <span class="stat-value">{{ stats.avg_early_days.toFixed(1) }} 天</span>
      </div>
      <div class="stat-block">
        <span class="stat-label">平均顺延</span>
        <span class="stat-value">{{ stats.avg_carryover_days.toFixed(1) }} 次</span>
      </div>
    </div>
    <HeatmapChart :data="heatmap" />
    <div class="weekly-trend">
      <h3>周完成趋势</h3>
      <div class="trend-chart">
        <div v-for="(w, i) in stats.weekly_trend" :key="i" class="trend-item">
          <div class="trend-bar" :style="{ height: barHeight(w.completed_count) + 'px' }">
            <span class="bar-value" v-if="w.completed_count > 0">{{ w.completed_count }}</span>
          </div>
          <span class="bar-label">-{{ w.week_offset }}周</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import api from '../api.js'
import HeatmapChart from '../components/HeatmapChart.vue'

const stats = ref({
  total_completed: 0, on_time_completed: 0, early_completed: 0,
  carried_over_count: 0, overdue_count: 0, completion_rate: 0,
  streak_days: 0, plan_health_score: 0, avg_early_days: 0,
  avg_carryover_days: 0, weekly_trend: []
})
const heatmap = ref({ weeks: [], max_daily_count: 0 })

async function load() {
  const r1 = await api.statsDfx()
  if (r1.code === 0) stats.value = { ...stats.value, ...r1.data }
  const r2 = await api.statsHeatmap()
  if (r2.code === 0) heatmap.value = r2.data
}

function barHeight(v) { return Math.max(4, Math.min(v * 16, 100)) }

onMounted(load)
</script>

<style scoped>
.stats-dfx-page { padding: 16px; background: #F3F3F3; min-height: 100vh; }
.stats-dfx-page h2 { margin: 0 0 16px 0; font-size: 1.4em; }
.summary-cards { display: grid; grid-template-columns: repeat(5, 1fr); gap: 12px; margin-bottom: 24px; }
.card { background: #fff; border-radius: 8px; padding: 16px; text-align: center; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.card-value { font-size: 2em; font-weight: bold; }
.card-label { color: #666; font-size: 0.85em; margin-top: 4px; }
.card.success .card-value { color: #4CAF50; }
.card.info .card-value { color: #2196F3; }
.card.warning .card-value { color: #FF9800; }
.card.danger .card-value { color: #E53935; }
.stats-row { display: flex; gap: 24px; margin-bottom: 24px; flex-wrap: wrap; background: #fff; padding: 16px; border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.stat-block { text-align: center; min-width: 80px; }
.stat-label { display: block; font-size: 0.85em; color: #666; margin-bottom: 4px; }
.stat-value { font-size: 1.2em; font-weight: bold; }
.weekly-trend { background: #fff; border-radius: 8px; padding: 16px; margin-top: 24px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.weekly-trend h3 { margin: 0 0 16px 0; }
.trend-chart { display: flex; gap: 8px; align-items: flex-end; height: 120px; }
.trend-item { display: flex; flex-direction: column; align-items: center; flex: 1; }
.trend-bar { background: #0078D4; width: 100%; max-width: 32px; border-radius: 4px 4px 0 0; position: relative; min-height: 4px; transition: height 0.3s; }
.bar-value { position: absolute; top: -18px; left: 50%; transform: translateX(-50%); font-size: 0.7em; color: #666; white-space: nowrap; }
.bar-label { font-size: 0.7em; color: #999; margin-top: 4px; }
</style>