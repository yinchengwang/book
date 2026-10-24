/**
 * 技术 —— 4 个模块聚合入口
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')

const ENTRIES = [
  {
    key: 'notes',
    symbol: 'md',
    title: '技术笔记',
    desc: 'Markdown 记录，支持标签与置顶',
    path: '/pages/tech/notes/index'
  },
  {
    key: 'snippets',
    symbol: '<>',
    title: '代码片段',
    desc: '可复用代码，按语言归类',
    path: '/pages/tech/snippets/index'
  },
  {
    key: 'interview',
    symbol: '?',
    title: '面试题库',
    desc: '题目与答案，带复习计数',
    path: '/pages/tech/interview/index'
  },
  {
    key: 'resources',
    symbol: '★',
    title: '资源收藏',
    desc: '文章、书籍与工具，带星级',
    path: '/pages/tech/resources/index'
  }
]

Page(withTheme({
  data: {
    title: '技术',
    entries: [],
    total: 0,
    captureVisible: false
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const counts = {
      notes: stores.techNotes.getAll().length,
      snippets: stores.snippets.getAll().length,
      interview: stores.interview.getAll().length,
      resources: stores.resources.getAll().length
    }

    let total = 0
    Object.keys(counts).forEach(function (k) { total += counts[k] })

    this.setData({
      entries: ENTRIES.map(function (e) {
        return {
          key: e.key,
          symbol: e.symbol,
          title: e.title,
          desc: e.desc,
          path: e.path,
          count: counts[e.key]
        }
      }),
      total: total
    })
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
    return { title: 'DevBench · 技术栈', path: '/pages/tech/index' }
  }
}))
