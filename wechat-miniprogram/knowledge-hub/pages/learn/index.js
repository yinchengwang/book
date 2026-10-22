/**
 * 学习 —— 4 个模块聚合入口
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')
const khc = require('../../core/kh-constants.js')

Page(withTheme({
  data: {
    entries: [],
    total: 0,
    plan: null
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const plan = stores.quizzes.getTodayPlan()
    const counts = {
      quiz: stores.quizzes.getQuestionBank().length,
      review: stores.reviews.getAll().length,
      deep: require('../../core/seed-data.js').LEARN_DEEP.length,
      path: stores.learningPaths.getAll().length
    }

    const entries = [
      {
        key: 'quiz', symbol: '?', title: '题库练习', color: '#6366F1',
        desc: '7 大技术栈 · 每日计划 · 错题本', path: '/pages/learn/quiz/index', count: counts.quiz
      },
      {
        key: 'review', symbol: 'R', title: '复习计划', color: '#0284C7',
        desc: 'SM-2 间隔重复 · 到期队列', path: '/pages/learn/review/index', count: counts.review
      },
      {
        key: 'deep', symbol: 'D', title: '图解系列', color: '#30A46C',
        desc: 'Markdown 长文 · 按栈分类', path: '/pages/learn/deep/index', count: counts.deep
      },
      {
        key: 'path', symbol: 'P', title: '学习路径', color: '#7F77DD',
        desc: '分步进阶 · 子任务进度', path: '/pages/learn/path/index', count: counts.path
      }
    ]

    let total = 0
    Object.keys(counts).forEach(function (k) { total += counts[k] })

    this.setData({
      entries: entries,
      total: total,
      plan: {
        stack: plan.stack,
        stackLabel: khc.stackMeta(plan.stack).label,
        done: plan.completed || 0,
        total: plan.questionIds.length
      }
    })
  },

  onOpen (e) {
    const path = e.currentTarget.dataset.path
    if (path) wx.navigateTo({ url: path })
  },

  onOpenPlan () {
    wx.navigateTo({ url: '/pages/learn/quiz/index' })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 学习', path: '/pages/learn/index' }
  }
}))