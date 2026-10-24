/**
 * 今日 —— 首屏
 *
 * 结构（自上而下，拇指热区优先）：
 *   日期 + 问候 / 今日待办（可勾选、可加、优先级可循环）/ 4 指标带 / 心情打卡 / 快捷入口
 *
 * 设计要点：无装饰性大卡，待办直接置顶；一屏内回答"今天什么状态"。
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')
const dateUtil = require('../../core/date.js')
const constants = require('../../core/constants.js')

const VISIBLE_TODOS = 3

const SHORTCUTS = [
  { key: 'notes', label: '笔记', path: '/pages/tech/notes/index' },
  { key: 'snippets', label: '片段', path: '/pages/tech/snippets/index' },
  { key: 'insights', label: '闪念', path: '/pages/insights/index' },
  { key: 'schedule', label: '日程', path: '/pages/life/schedule/index' },
  { key: 'projects', label: '项目', path: '/pages/projects/index' }
]

Page(withTheme({
  data: {
    title: '今日',

    dateLabel: '',
    greeting: '',
    nickname: '',

    todoItems: [],
    todoOpenCount: 0,
    todoTotal: 0,
    todoMoreCount: 0,
    showAllTodos: false,
    hasTodos: false,

    stats: [],
    moodLevels: [],
    moodToday: 0,
    moodLabel: '',

    shortcuts: SHORTCUTS,
    captureVisible: false
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  /** 汇总所有首屏数据 */
  refresh () {
    const settings = stores.settings.get()
    const openTodos = stores.todos.getOpen()
    const allTodos = stores.todos.getAll()
    const limit = this.data.showAllTodos ? openTodos.length : VISIBLE_TODOS
    const visible = openTodos.slice(0, limit)

    const todoItems = visible.map(function (t) {
      const p = constants.priority(t.priority)
      return {
        id: t.id,
        title: t.title,
        done: !!t.done,
        barColor: p.color,
        priorityLabel: p.label,
        categoryLabel: t.category ? constants.todoCategory(t.category).label : ''
      }
    })

    const moodEntries = stores.mood.getAll()
    const today = dateUtil.todayStr()
    let moodToday = 0
    for (let i = 0; i < moodEntries.length; i++) {
      if (moodEntries[i].date === today) moodToday = moodEntries[i].level
    }

    this.setData({
      dateLabel: dateUtil.todayLabel(),
      greeting: dateUtil.greeting(),
      nickname: settings.profile.nickname || 'DevBencHer',

      todoItems: todoItems,
      todoOpenCount: openTodos.length,
      todoTotal: allTodos.length,
      todoMoreCount: Math.max(0, openTodos.length - VISIBLE_TODOS),
      hasTodos: allTodos.length > 0,

      stats: this.buildStats(),
      moodLevels: this.buildMoodLevels(moodToday),
      moodToday: moodToday,
      moodLabel: moodToday ? constants.moodLevel(moodToday).label : '未记录'
    })
  },

  /** 4 个状态指标 */
  buildStats () {
    const notes = stores.techNotes.getAll().length
    const readingNow = stores.reading.count(function (r) { return r.status === 'reading' })
    const week = dateUtil.weekRange()
    const fitness = stores.fitness.getAll()
    const days = {}
    fitness.forEach(function (r) {
      if (r.date >= week.start && r.date <= week.end) days[r.date] = true
    })

    return [
      { key: 'notes', value: String(notes), label: '笔记', highlight: false },
      { key: 'reading', value: String(readingNow), label: '在读书', highlight: false },
      { key: 'fitness', value: String(Object.keys(days).length), label: '本周运动', highlight: false },
      { key: 'streak', value: String(this.calcStreak()), label: '连续天', highlight: true }
    ]
  },

  /**
   * 连续记录天数：从今天（或昨天）往前数有心情记录的连续天数
   * 今天未记录不算断，从昨天开始算
   */
  calcStreak () {
    const entries = stores.mood.getAll()
    const set = {}
    entries.forEach(function (e) { set[e.date] = true })

    const cursor = new Date()
    if (!set[dateUtil.todayStr()]) {
      cursor.setDate(cursor.getDate() - 1)
    }

    let streak = 0
    // 上限 3650 天，防止脏数据导致死循环
    for (let i = 0; i < 3650; i++) {
      const d = dateUtil.formatYMD(cursor)
      if (!set[d]) break
      streak += 1
      cursor.setDate(cursor.getDate() - 1)
    }
    return streak
  },

  /** 5 级心情圆点 */
  buildMoodLevels (current) {
    return constants.MOOD_LEVELS.map(function (m) {
      return {
        level: m.level,
        label: m.label,
        color: m.color,
        active: m.level === current
      }
    })
  },

  // ==================== 待办 ====================

  onToggleTodo (e) {
    const id = e.currentTarget.dataset.id
    stores.todos.toggle(id)
    this.refresh()
    wx.vibrateShort({ type: 'light' })
  },

  /** 点优先级圆点循环切换 高 → 中 → 低 */
  onCyclePriority (e) {
    const id = e.currentTarget.dataset.id
    const item = stores.todos.find(id)
    if (!item) return
    const order = ['high', 'med', 'low']
    const next = order[(order.indexOf(item.priority) + 1) % order.length]
    stores.todos.update(id, { priority: next })
    this.refresh()
    wx.showToast({ title: '优先级：' + constants.priority(next).label, icon: 'none' })
  },

  onAddTodo () {
    const self = this
    wx.showModal({
      title: '添加待办',
      editable: true,
      placeholderText: '要做什么？',
      success: function (res) {
        if (!res.confirm) return
        const title = (res.content || '').trim()
        if (!title) return
        stores.todos.add({
          title: title,
          done: false,
          priority: 'med',
          category: 'work',
          dueDate: dateUtil.todayStr()
        })
        self.setData({ showAllTodos: true })
        self.refresh()
      }
    })
  },

  onToggleAllTodos () {
    this.setData({ showAllTodos: !this.data.showAllTodos })
    this.refresh()
  },

  // ==================== 心情 ====================

  onSetMood (e) {
    const level = Number(e.currentTarget.dataset.level)
    stores.mood.upsertToday({ level: level, tags: [] })
    this.refresh()
    wx.vibrateShort({ type: 'light' })
    wx.showToast({ title: '已记录：' + constants.moodLevel(level).label, icon: 'none' })
  },

  // ==================== 导航 ====================

  onGoShortcut (e) {
    const path = e.currentTarget.dataset.path
    if (path) wx.navigateTo({ url: path })
  },

  onGoProfile () {
    wx.switchTab({ url: '/pages/profile/index' })
  },

  // ==================== 捕获浮层 ====================

  onOpenCapture () {
    this.setData({ captureVisible: true })
  },

  onCloseCapture () {
    this.setData({ captureVisible: false })
  },

  onCaptured () {
    this.refresh()
  },

  onShareAppMessage () {
    return { title: 'DevBench · 程序员个人工作台', path: '/pages/dashboard/index' }
  }
}))
