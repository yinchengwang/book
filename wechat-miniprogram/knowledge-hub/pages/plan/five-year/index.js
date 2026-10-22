/**
 * 五年计划 —— 年份切换 / 月历网格 / 习惯打卡 / 连续天数
 *
 * 月历是纯 View 网格（非canvas），原版即可直接复刻。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const khc = require('../../../core/kh-constants.js')
const date = require('../../../core/date.js')
const seedData = require('../../../core/seed-data.js')

const WEEK = ['日', '一', '二', '三', '四', '五', '六']

Page(withTheme({
  data: {
    years: seedData.ALL_YEARS,
    year: seedData.ALL_YEARS[0],
    month: 1,
    weeks: WEEK,
    days: [],
    habits: [],
    config: null,
    streak: 0,
    totalDays: 0,
    monthPct: 0,
    current: null,
    showEdit: false,
    editForm: { note: '', hasLearning: false },
    projects: []
  },

  onLoad () {
    const now = new Date()
    this.setData({ year: now.getFullYear(), month: now.getMonth() + 1 })
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const year = this.data.year
    const month = this.data.month
    const config = seedData.getYearConfig(year)

    // 生成月历网格（补齐前后空位）
    const first = new Date(year, month - 1, 1)
    const daysInMonth = new Date(year, month, 0).getDate()
    const lead = first.getDay()
    const cells = []
    for (let i = 0; i < lead; i++) cells.push(null)
    for (let d = 1; d <= daysInMonth; d++) {
      const checks = stores.fiveYearPlan.getDay(year, month, d)
      const done = checks ? Object.keys(checks.checks || {}).filter(function (k) { return checks.checks[k] }).length : 0
      const total = config ? config.habits.length : 0
      cells.push({
        day: d,
        done: done,
        total: total,
        ratio: total ? done / total : 0,
        fillPct: total ? Math.round(done / total * 100) : 0,
        hasLearning: checks ? !!checks.hasLearning : false,
        isToday: date.formatYMD(new Date(year, month - 1, d)) === date.todayStr()
      })
    }
    while (cells.length % 7 !== 0) cells.push(null)

    const days = []
    for (let i = 0; i < cells.length; i += 7) days.push(cells.slice(i, i + 7))

    this.setData({
      days: days,
      config: config,
      habits: config ? config.habits : [],
      streak: stores.fiveYearPlan.getStreak(),
      totalDays: stores.fiveYearPlan.getTotalDays(),
      monthPct: stores.fiveYearPlan.getMonthPct(year, month),
      projects: seedData.CORE_PROJECTS.filter(function (p) { return p.year === year })
    })
  },

  onShiftYear (e) {
    const delta = Number(e.currentTarget.dataset.delta)
    this.setData({ year: this.data.year + delta })
    this.refresh()
  },

  onShiftMonth (e) {
    const delta = Number(e.currentTarget.dataset.delta)
    let m = this.data.month + delta
    let y = this.data.year
    if (m < 1) { m = 12; y -= 1 }
    if (m > 12) { m = 1; y += 1 }
    this.setData({ month: m, year: y })
    this.refresh()
  },

  /** 今日快速打卡 */
  onQuickCheck (e) {
    const ds = e.currentTarget.dataset
    const now = new Date()
    stores.fiveYearPlan.toggleCheck(this.data.year, this.data.month, now.getDate(), ds.habit)
    stores.fiveYearPlan.toggleLearning(this.data.year, this.data.month, now.getDate())
    this.refresh()
  },

  onOpenDay (e) {
    const day = Number(e.currentTarget.dataset.day)
    const checks = stores.fiveYearPlan.getDay(this.data.year, this.data.month, day)
    this.setData({
      current: day,
      showEdit: true,
      editForm: {
        note: checks ? checks.note : '',
        hasLearning: checks ? !!checks.hasLearning : false
      }
    })
  },

  onCloseEdit () {
    this.setData({ showEdit: false, current: null })
  },

  onEditInput (e) {
    this.setData({ 'editForm.note': e.detail.value })
  },

  onToggleLearning () {
    stores.fiveYearPlan.toggleLearning(this.data.year, this.data.month, this.data.current)
    const checks = stores.fiveYearPlan.getDay(this.data.year, this.data.month, this.data.current)
    this.setData({ 'editForm.hasLearning': checks ? !!checks.hasLearning : false })
  },

  onSaveDay () {
    stores.fiveYearPlan.setDay(this.data.year, this.data.month, this.data.current, {
      note: this.data.editForm.note
    })
    this.setData({ showEdit: false, current: null })
    this.refresh()
  },

  onDeleteDay () {
    stores.fiveYearPlan.deleteDay(this.data.year, this.data.month, this.data.current)
    this.setData({ showEdit: false, current: null })
    this.refresh()
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 五年计划', path: '/pages/plan/five-year/index' }
  }
}))