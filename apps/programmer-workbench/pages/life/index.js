/**
 * 生活 —— 4 个模块聚合入口
 *
 * 每项显示一条最关键的摘要，让用户不进去也知道状态。
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')
const dateUtil = require('../../core/date.js')

Page(withTheme({
  data: {
    title: '生活',
    entries: [],
    captureVisible: false
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const reading = stores.reading.getAll()
    const readingNow = reading.filter(function (r) { return r.status === 'reading' }).length

    const week = dateUtil.weekRange()
    const fitness = stores.fitness.getAll().filter(function (r) {
      return r.date >= week.start && r.date <= week.end
    })
    let weekMinutes = 0
    fitness.forEach(function (r) { weekMinutes += (r.durationMin || 0) })

    const today = dateUtil.todayStr()
    const todayEvents = stores.schedule.getAll().filter(function (e) {
      return String(e.startAt || '').slice(0, 10) === today
    }).length

    this.setData({
      entries: [
        {
          key: 'reading',
          symbol: '书',
          title: '读书',
          desc: '在读 ' + readingNow + ' 本 · 共 ' + reading.length + ' 本',
          count: readingNow,
          path: '/pages/life/reading/index'
        },
        {
          key: 'fitness',
          symbol: '练',
          title: '健身',
          desc: '本周 ' + fitness.length + ' 次 · ' + weekMinutes + ' 分钟',
          count: fitness.length,
          path: '/pages/life/fitness/index'
        },
        {
          key: 'mood',
          symbol: '心',
          title: '心情',
          desc: '已连续记录 ' + this.calcStreak() + ' 天',
          count: this.calcStreak(),
          path: '/pages/life/mood/index'
        },
        {
          key: 'schedule',
          symbol: '程',
          title: '日程',
          desc: '今天 ' + todayEvents + ' 项安排',
          count: todayEvents,
          path: '/pages/life/schedule/index'
        }
      ]
    })
  },

  /** 连续心情记录天数（今天未记不算断） */
  calcStreak () {
    const entries = stores.mood.getAll()
    const set = {}
    entries.forEach(function (e) { set[e.date] = true })

    const cursor = new Date()
    if (!set[dateUtil.todayStr()]) cursor.setDate(cursor.getDate() - 1)

    let streak = 0
    for (let i = 0; i < 3650; i++) {
      const d = dateUtil.formatYMD(cursor)
      if (!set[d]) break
      streak += 1
      cursor.setDate(cursor.getDate() - 1)
    }
    return streak
  },

  onOpen (e) {
    const path = e.currentTarget.dataset.path
    if (path) wx.navigateTo({ url: path })
  },

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
    return { title: 'DevBench · 生活', path: '/pages/life/index' }
  }
}))
