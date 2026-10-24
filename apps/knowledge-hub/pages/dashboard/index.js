/**
 * 首页仪表盘 —— 学习概览、今日任务、统计、技术栈进度、快捷入口
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')
const date = require('../../core/date.js')
const khc = require('../../core/kh-constants.js')
const seedData = require('../../core/seed-data.js')

Page(withTheme({
  data: {
    greeting: '',
    todayLabel: '',
    stats: [],
    dueCount: 0,
    streak: 0,
    plan: null,
    planProgress: '',
    stackProgress: [],
    gaps: [],
    shortcuts: [],
    hasActivity: false
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const user = stores.user.get()
    const quizStats = stores.quizzes.getStats()
    const due = stores.reviews.getDue()
    const streak = stores.fiveYearPlan.getStreak()
    const plan = stores.quizzes.getTodayPlan()

    const gaps = stores.knowledge.getGaps()
    const gapRows = Object.keys(gaps).map(function (k) {
      const g = gaps[k]
      const meta = khc.KNOWLEDGE_DOMAINS.filter(function (d) { return d.key === k })[0]
      return {
        key: k,
        label: meta ? meta.label : k,
        color: meta ? meta.color : '#64748B',
        level: g.level,
        target: g.target,
        percent: g.target ? Math.min(100, Math.round(g.level / g.target * 100)) : 0
      }
    }).sort(function (a, b) { return a.percent - b.percent }).slice(0, 3)

    const stackProgress = khc.STACK_ORDER.map(function (s) {
      const meta = khc.stackMeta(s)
      const prog = stores.quizzes.getProgress(s)
      return {
        key: s,
        label: meta.label,
        icon: meta.icon,
        color: meta.color,
        total: prog.total,
        answered: prog.answered,
        percent: prog.total ? Math.round(prog.answered / prog.total * 100) : 0
      }
    })

    this.setData({
      greeting: date.greeting() + '，' + (user.name || '同学'),
      todayLabel: date.todayLabel(),
      dueCount: due.length,
      streak: streak,
      plan: { stack: plan.stack, stackLabel: khc.stackMeta(plan.stack).label, total: plan.questionIds.length, completed: plan.completed || 0 },
      planProgress: plan.questionIds.length ? Math.round((plan.completed || 0) / plan.questionIds.length * 100) : 0,
      stackProgress: stackProgress,
      gaps: gapRows,
      hasActivity: quizStats.totalAnswered > 0 || due.length > 0,
      stats: [
        { key: 'answered', label: '累计答题', value: String(quizStats.totalAnswered), color: '#6366F1' },
        { key: 'accuracy', label: '正确率', value: Math.round(quizStats.accuracy * 100) + '%', color: '#30A46C' },
        { key: 'streak', label: '连续打卡', value: String(streak), color: '#F5A524' },
        { key: 'due', label: '待复习', value: String(due.length), color: '#E5484D' },
        { key: 'paths', label: '学习路径', value: String(stores.learningPaths.getAll().length), color: '#0284C7' }
      ],
      shortcuts: [
        { key: 'quiz', symbol: '?', title: '题库练习', path: '/pages/learn/quiz/index', color: '#6366F1' },
        { key: 'review', symbol: 'R', title: '复习计划', path: '/pages/learn/review/index', color: '#0284C7' },
        { key: 'deep', symbol: 'D', title: '图解系列', path: '/pages/learn/deep/index', color: '#30A46C' },
        { key: 'path', symbol: 'P', title: '学习路径', path: '/pages/learn/path/index', color: '#7F77DD' }
      ]
    })
  },

  onOpen (e) {
    const path = e.currentTarget.dataset.path
    if (!path) return
    wx.navigateTo({ url: path })
  },

  onOpenPlan () {
    wx.navigateTo({ url: '/pages/learn/quiz/index' })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 学习概览', path: '/pages/dashboard/index' }
  }
}))