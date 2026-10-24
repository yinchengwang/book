/**
 * 面试题库 —— 算法 / 后端两类，答案用 rich-text 渲染 Markdown
 *
 * 打开即记录「已浏览」，可收藏；题库内容来自 seed-data.INTERVIEW_BANK（静态）。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const md = require('../../../core/md.js')
const seedData = require('../../../core/seed-data.js')

Page(withTheme({
  data: {
    banks: [
      { key: 'algorithm', label: '算法' },
      { key: 'interview', label: '后端' }
    ],
    bank: 'algorithm',
    categories: [],
    category: '',
    query: '',
    list: [],
    current: null,
    html: '',
    showBookmarks: false,
    bookmarkCount: 0
  },

  onLoad () {
    this.refresh()
  },

  refresh () {
    const source = seedData.INTERVIEW_BANK[this.data.bank]
    const categories = []
    source.forEach(function (q) {
      if (categories.indexOf(q.category) < 0) categories.push(q.category)
    })

    const q = (this.data.query || '').trim().toLowerCase()
    const state = stores.interviewBank.get()
    let list = source
    if (this.data.showBookmarks) list = list.filter(function (item) { return state.bookmarks.indexOf(item.id) >= 0 })
    if (this.data.category) list = list.filter(function (item) { return item.category === this.data.category }, this)
    if (q) list = list.filter(function (item) { return item.question.toLowerCase().indexOf(q) >= 0 })

    this.setData({
      categories: categories,
      viewedCount: state.viewed.length,
      bookmarkCount: state.bookmarks.length,
      list: list.map(function (item) {
        return {
          id: item.id,
          question: item.question,
          category: item.category,
          difficulty: item.difficulty,
          viewed: state.viewed.indexOf(item.id) >= 0,
          bookmarked: state.bookmarks.indexOf(item.id) >= 0
        }
      })
    })
  },

  onSwitchBank (e) {
    this.setData({ bank: e.currentTarget.dataset.key, category: '' })
    this.refresh()
  },

  onPickCategory (e) {
    const cat = e.currentTarget.dataset.cat
    this.setData({ category: this.data.category === cat ? '' : cat })
    this.refresh()
  },

  onSearch (e) {
    this.setData({ query: e.detail.value })
    this.refresh()
  },

  onToggleBookmarks () {
    this.setData({ showBookmarks: !this.data.showBookmarks })
    this.refresh()
  },

  onOpen (e) {
    const id = e.currentTarget.dataset.id
    const source = seedData.INTERVIEW_BANK[this.data.bank]
    let found = null
    for (let i = 0; i < source.length; i++) {
      if (source[i].id === id) found = source[i]
    }
    if (!found) return
    stores.interviewBank.markViewed(id)
    this.setData({
      current: {
        id: found.id,
        question: found.question,
        category: found.category,
        difficulty: found.difficulty,
        bookmarked: stores.interviewBank.isBookmarked(id)
      },
      html: md.render(found.answer, this.data.isDark)
    })
    this.refresh()
  },

  onToggleBookmark (e) {
    const id = e.currentTarget.dataset.id
    stores.interviewBank.toggleBookmark(id)
    this.refresh()
    const cur = this.data.current
    if (cur && cur.id === id) this.setData({ 'current.bookmarked': stores.interviewBank.isBookmarked(id) })
  },

  onClose () {
    this.setData({ current: null, html: '' })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 面试题库', path: '/pages/interview/bank/index' }
  }
}))