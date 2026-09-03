/**
 * 五年建设计划
 *
 * 月历视图 + 16 习惯项 + 月度学习 + 笔记/复盘
 * 复刻自旧版 five-year-plan.html
 */
import { useState, useMemo, useEffect } from 'react'
import { View, Text, Input, Textarea } from '@tarojs/components'
import {
  YEAR_CONFIG, ALL_YEARS, CORE_PROJECTS,
  getAllItems, getGroupedItems, HabitItem
} from '@/data/five_year_plan_config'
import { useFiveYearPlanStore } from '@/stores/fiveYearPlanStore'
import './five_year_plan.scss'

// 一周标签
const WEEKDAYS = ['日', '一', '二', '三', '四', '五', '六']

// 月份英文/中文
const MONTH_NAMES = ['', '1月', '2月', '3月', '4月', '5月', '6月', '7月', '8月', '9月', '10月', '11月', '12月']

// 工具
const pad = (n: number) => String(n).padStart(2, '0')
const dateKey = (y: number, m: number, d: number) => `${y}-${pad(m)}-${pad(d)}`

const getToday = () => {
  const d = new Date()
  return { year: d.getFullYear(), month: d.getMonth() + 1, day: d.getDate() }
}

// 获取当月所有格子（含前置空白）
function getMonthCells(year: number, month: number) {
  const firstDay = new Date(year, month - 1, 1).getDay()
  const daysInMonth = new Date(year, month, 0).getDate()
  const cells: Array<{ day: number | null; dateKey: string | null }> = []
  for (let i = 0; i < firstDay; i++) cells.push({ day: null, dateKey: null })
  for (let d = 1; d <= daysInMonth; d++) cells.push({ day: d, dateKey: dateKey(year, month, d) })
  return cells
}

const FiveYearPlan = () => {
  const today = getToday()
  const [currentYear, setCurrentYear] = useState(today.year)
  const [currentMonth, setCurrentMonth] = useState(today.month)
  const [modalDate, setModalDate] = useState<string | null>(null)

  const {
    data,
    getDay, setDay, deleteDay, toggleCheck, toggleLearning,
    countDayChecks, getStreak, getTotalDays, getMonthPct
  } = useFiveYearPlanStore()

  // 当 modalDate 设置时读取数据
  const editingDayData = useMemo(() => {
    if (!modalDate) return null
    const [y, m, d] = modalDate.split('-').map(Number)
    return getDay(y, m, d)
  }, [modalDate, data, getDay])

  // 编辑器内部状态
  const [editChecks, setEditChecks] = useState<Record<string, boolean>>({})
  const [editNote, setEditNote] = useState('')
  const [editHasLearning, setEditHasLearning] = useState(false)

  // 当打开 modal 时同步编辑器状态
  useEffect(() => {
    if (editingDayData && modalDate) {
      setEditChecks(editingDayData.checks)
      setEditNote(editingDayData.note)
      setEditHasLearning(!!editingDayData.hasLearning)
    }
  }, [editingDayData, modalDate])

  // 年份切换
  const switchYear = (y: number) => {
    setCurrentYear(y)
    if (y > today.year) setCurrentMonth(1)
    else if (y === today.year) setCurrentMonth(today.month)
  }

  // 月份切换
  const changeMonth = (delta: number) => {
    let m = currentMonth + delta
    let y = currentYear
    if (m > 12) {
      if (y < 2030) { y++; m = 1 }
      else m = 12
    } else if (m < 1) {
      if (y > 2026) { y--; m = 12 }
      else m = 1
    }
    setCurrentYear(y)
    setCurrentMonth(m)
  }

  const goToToday = () => {
    setCurrentYear(today.year)
    setCurrentMonth(today.month)
  }

  // 快速切换（今日面板）
  const quickToggle = (habitId: string) => {
    toggleCheck(today.year, today.month, today.day, habitId)
  }

  // 打开编辑弹窗
  const openModal = (dk: string) => setModalDate(dk)
  const closeModal = () => setModalDate(null)

  // 保存编辑
  const saveModal = () => {
    if (!modalDate) return
    const [y, m, d] = modalDate.split('-').map(Number)
    setDay(y, m, d, { checks: editChecks, note: editNote, hasLearning: editHasLearning })
    closeModal()
  }

  // 清除
  const onDelete = () => {
    if (!modalDate) return
    if (!window.confirm('确定要清除这一天的所有打卡记录吗？')) return
    const [y, m, d] = modalDate.split('-').map(Number)
    deleteDay(y, m, d)
    closeModal()
  }

  // 编辑器中切换某项
  const toggleEditCheck = (habitId: string) => {
    setEditChecks(prev => ({ ...prev, [habitId]: !prev[habitId] }))
  }

  // 统计数据
  const todayChecks = countDayChecks(today.year, today.month, today.day)
  const todayPct = todayChecks.total > 0 ? Math.round(todayChecks.done / todayChecks.total * 100) : 0
  const monthPct = getMonthPct(currentYear, currentMonth)
  const streak = getStreak()
  const totalDays = getTotalDays()

  // 当前年份配置
  const yearCfg = YEAR_CONFIG[currentYear] || YEAR_CONFIG[2026]
  const yearExtras = yearCfg.extras

  // 弹窗日期信息
  const modalInfo = modalDate ? {
    y: parseInt(modalDate.split('-')[0], 10),
    m: parseInt(modalDate.split('-')[1], 10),
    d: parseInt(modalDate.split('-')[2], 10)
  } : null

  // 日历格子
  const cells = getMonthCells(currentYear, currentMonth)
  const groupedItems = getGroupedItems(currentYear)
  const allItems = getAllItems(currentYear)

  return (
    <View className="fyp-page">
      {/* 头部 */}
      <View className="fyp-header">
        <Text className="fyp-title">🏗️ 五年建设计划</Text>
        <Text className="fyp-subtitle">稳根基 · 长续航 · 向高处 — 像建设国家一样建设自己</Text>
      </View>

      {/* 年份导航 */}
      <View className="year-nav">
        {ALL_YEARS.map(y => {
          const cfg = YEAR_CONFIG[y]
          const active = y === currentYear
          return (
            <View
              key={y}
              className={`year-btn ${cfg.cls}${active ? ' active' : ''}`}
              onClick={() => switchYear(y)}
            >
              <Text className="year-dot" />
              <Text>{y} {cfg.name}</Text>
            </View>
          )
        })}
      </View>

      <View className="fyp-main">
        {/* 年度主题卡 */}
        <View className={`theme-card ${yearCfg.cls}`}>
          <View className="theme-header">
            <Text className="core-tag">{yearCfg.core}</Text>
            <Text className="theme-title">{currentYear} · {yearCfg.name}</Text>
          </View>
          <View className="actions">
            {yearCfg.actions.map((a, i) => (
              <View key={i} className="action-item">▸ {a}</View>
            ))}
          </View>
        </View>

        {/* 统计面板 */}
        <View className="stats-row">
          <View className={`stat-box ${todayPct >= 70 ? 'good' : 'warn'}`}>
            <Text className="num">{todayPct}%</Text>
            <Text className="label">今日完成率</Text>
          </View>
          <View className={`stat-box ${monthPct >= 60 ? 'good' : 'warn'}`}>
            <Text className="num">{monthPct}%</Text>
            <Text className="label">本月完成率</Text>
          </View>
          <View className="stat-box good">
            <Text className="num">{streak}</Text>
            <Text className="label">连续打卡 (天)</Text>
          </View>
          <View className="stat-box good">
            <Text className="num">{totalDays}</Text>
            <Text className="label">累计打卡 (天)</Text>
          </View>
        </View>

        {/* 今日快速打卡 */}
        <View className="today-panel">
          <Text className="today-tag">今天</Text>
          <Text className="today-title">📅 今日打卡 · {today.year}年{today.month}月{today.day}日</Text>
          <View className="quick-checks">
            {allItems.map(item => {
              const isDone = editingDayData?.checks?.[item.id] && modalDate === dateKey(today.year, today.month, today.day)
              // 今日 panel 的勾选状态读自 store 而非编辑态
              return (
                <TodayChip
                  key={item.id}
                  item={item}
                  isDone={!!getDay(today.year, today.month, today.day).checks[item.id]}
                  onClick={() => quickToggle(item.id)}
                />
              )
            })}
          </View>
          <View className="today-actions">
            <View className="btn-edit" onClick={() => openModal(dateKey(today.year, today.month, today.day))}>
              <Text>📝 详细编辑</Text>
            </View>
          </View>
        </View>

        {/* 月份导航 */}
        <View className="month-nav">
          <View className="month-btn" onClick={() => changeMonth(-1)}>
            <Text>◀ 上月</Text>
          </View>
          <Text className="month-label">{currentYear}年 {currentMonth}月</Text>
          <View className="month-btn" onClick={() => changeMonth(1)}>
            <Text>下月 ▶</Text>
          </View>
          <View className="month-btn today-btn" onClick={goToToday}>
            <Text>📍 今天</Text>
          </View>
        </View>

        {/* 日历 */}
        <View className="weekday-header">
          {WEEKDAYS.map(w => <Text key={w} className="weekday">{w}</Text>)}
        </View>
        <View className="calendar-grid">
          {cells.map((cell, i) => {
            if (cell.day === null) {
              return <View key={`empty-${i}`} className="day-cell empty" />
            }
            const isToday = currentYear === today.year && currentMonth === today.month && cell.day === today.day
            const d = cell.day!
            const c = countDayChecks(currentYear, currentMonth, d)
            const dayData = getDay(currentYear, currentMonth, d)
            const hasNote = dayData.note && dayData.note.trim()

            return (
              <View
                key={cell.dateKey ?? `empty-${i}`}
                className={`day-cell${isToday ? ' today' : ''}`}
                onClick={() => cell.dateKey && openModal(cell.dateKey)}
              >
                <Text className="day-num">{d}</Text>
                <View className="dot-row">
                  {/* 5 大核心工程点 */}
                  {CORE_PROJECTS.map(proj => {
                    let projDone = 0
                    for (const item of proj.items) {
                      if (dayData.checks[item.id]) projDone++
                    }
                    const projTotal = proj.items.length
                    if (projTotal === 0) return null
                    const cls = projDone === projTotal ? 'done' : (projDone > 0 ? 'partial' : 'miss')
                    return <Text key={proj.id} className={`dot ${cls}`} />
                  })}
                  {/* 年度专项点 */}
                  {yearExtras.length > 0 && (() => {
                    let extraDone = 0
                    for (const item of yearExtras) {
                      if (dayData.checks[item.id]) extraDone++
                    }
                    const cls = extraDone === yearExtras.length ? 'done' : (extraDone > 0 ? 'partial' : 'miss')
                    return <Text key="extra" className={`dot ${cls}`} />
                  })()}
                  {/* 学习标记点 */}
                  <Text className={`dot ${dayData.hasLearning ? 'learn' : 'learn-miss'}`} />
                </View>
                {hasNote && <Text className="extra-tag">📝</Text>}
              </View>
            )
          })}
        </View>

        {/* 图例 */}
        <View className="legend">
          <Text className="legend-item"><Text className="dot done" /> 全部完成</Text>
          <Text className="legend-item"><Text className="dot partial" /> 部分完成</Text>
          <Text className="legend-item"><Text className="dot miss" /> 未打卡</Text>
          <Text className="legend-item"><Text className="dot learn" /> 有学习</Text>
        </View>
      </View>

      {/* 弹窗 */}
      {modalDate && modalInfo && (
        <View className="modal-overlay" onClick={closeModal}>
          <View className="modal" onClick={(e) => e.stopPropagation()}>
            <Text className="modal-title">📋 {modalInfo.y}年{modalInfo.m}月{modalInfo.d}日 打卡记录</Text>

            {groupedItems.map(group => (
              <View key={group.label} className="check-group">
                <Text className="group-label">{group.label}</Text>
                {group.items.map(item => (
                  <View key={item.id} className="check-item" onClick={() => toggleEditCheck(item.id)}>
                    <View className={`checkbox${editChecks[item.id] ? ' checked' : ''}`}>
                      {editChecks[item.id] && <Text className="check-mark">✓</Text>}
                    </View>
                    <View className="item-text">
                      <Text className="item-label">{item.label}</Text>
                      <Text className="item-desc">{item.desc}</Text>
                    </View>
                  </View>
                ))}
              </View>
            ))}

            {/* 学习标记 */}
            <View className="check-group">
              <Text className="group-label">📚 今日学习</Text>
              <View className="check-item" onClick={() => setEditHasLearning(!editHasLearning)}>
                <View className={`checkbox${editHasLearning ? ' checked' : ''}`}>
                  {editHasLearning && <Text className="check-mark">✓</Text>}
                </View>
                <View className="item-text">
                  <Text className="item-label">标记为今日有学习（蓝色 dot）</Text>
                  <Text className="item-desc">切换该天的学习标记点</Text>
                </View>
              </View>
            </View>

            <Textarea
              className="note-area"
              placeholder="✍️ 今日笔记/复盘..."
              value={editNote}
              onInput={(e: any) => setEditNote(e.detail.value)}
            />

            <View className="btn-row">
              <View className="btn-delete" onClick={onDelete}>
                <Text>🗑️ 清除</Text>
              </View>
              <View className="btn-close" onClick={closeModal}>
                <Text>取消</Text>
              </View>
              <View className="btn-save" onClick={saveModal}>
                <Text>💾 保存</Text>
              </View>
            </View>
          </View>
        </View>
      )}
    </View>
  )
}

// 今日快速 chip 子组件
const TodayChip = ({ item, isDone, onClick }: { item: HabitItem; isDone: boolean; onClick: () => void }) => (
  <View
    className={`quick-chip${isDone ? ' done' : ''}`}
    onClick={onClick}
  >
    <Text>{isDone ? '✅' : '⬜'} {item.label}</Text>
  </View>
)

export default FiveYearPlan
