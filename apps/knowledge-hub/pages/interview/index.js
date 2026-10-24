/**
 * 面试 —— 3 个模块聚合入口
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')

Page(withTheme({
  data: {
    entries: [],
    stats: null,
    upcoming: []
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const derive = stores.interviews.derive()
    const companies = stores.interviews.getAll()
    const mocks = stores.mockInterviews.getAll()
    const bank = require('../../core/seed-data.js').INTERVIEW_BANK
    const bookmarkCount = stores.interviewBank.get().bookmarks.length

    const upcoming = companies
      .filter(function (c) { return c.status !== 'offer' && c.status !== '拒绝' })
      .slice(0, 3)
      .map(function (c) {
        const meta = require('../../core/kh-constants.js').interviewStatus(c.status)
        return {
          id: c.id,
          name: c.name,
          position: c.position,
          status: c.status,
          color: meta.color,
          progress: meta.progress
        }
      })

    this.setData({
      stats: { companies: derive.total, active: derive.active, mocks: mocks.length, bookmarks: bookmarkCount },
      upcoming: upcoming,
      entries: [
        {
          key: 'tracker', symbol: 'T', title: '面试追踪', color: '#6366F1',
          desc: '公司 · 状态流转 · 时间线', path: '/pages/interview/tracker/index', count: derive.total
        },
        {
          key: 'bank', symbol: 'Q', title: '面试题库', color: '#0284C7',
          desc: '算法 + 后端题 · 收藏与浏览记录', path: '/pages/interview/bank/index', count: bank.algorithm.length + bank.interview.length
        },
        {
          key: 'mock', symbol: 'M', title: '模拟面试', color: '#7F77DD',
          desc: '场景答题 · 评分报告', path: '/pages/interview/mock/index', count: mocks.length
        }
      ]
    })
  },

  onOpen (e) {
    const path = e.currentTarget.dataset.path
    if (path) wx.navigateTo({ url: path })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 面试', path: '/pages/interview/index' }
  }
}))