/**
 * 面试题库 · 列表
 *
 * 排序：收藏优先，其次按创建时间倒序。
 * 长按卡片可收藏 / 取消收藏 / 删除。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const constants = require('../../../core/constants.js')

Page(withTheme({
  data: {
    title: '面试题库',
    items: [],
    count: 0,
    starredCount: 0
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const list = stores.interview.getAll().slice()

    list.sort(function (a, b) {
      if (!!a.starred !== !!b.starred) return a.starred ? -1 : 1
      return new Date(b.createdAt || 0).getTime() - new Date(a.createdAt || 0).getTime()
    })

    const items = list.map(function (q) {
      const cat = constants.techCategory(q.category)
      const diff = constants.difficulty(q.difficulty)
      return {
        id: q.id,
        question: q.question,
        color: cat.color,
        categoryLabel: cat.label,
        difficultyLabel: diff.label,
        difficultyColor: diff.color,
        reviewCount: q.reviewCount || 0,
        starred: !!q.starred
      }
    })

    this.setData({
      items: items,
      count: items.length,
      starredCount: items.filter(function (i) { return i.starred }).length
    })
  },

  onOpen (e) {
    wx.navigateTo({ url: '/pages/tech/interview/detail?id=' + e.currentTarget.dataset.id })
  },

  onCreate () {
    wx.navigateTo({ url: '/pages/tech/interview/detail' })
  },

  onLongPress (e) {
    const id = e.currentTarget.dataset.id
    const item = stores.interview.find(id)
    if (!item) return

    const self = this
    wx.showActionSheet({
      itemList: [item.starred ? '取消收藏' : '收藏', '删除'],
      success: function (res) {
        if (res.tapIndex === 0) {
          stores.interview.update(id, { starred: !item.starred })
          self.refresh()
          wx.showToast({ title: item.starred ? '已取消收藏' : '已收藏', icon: 'none' })
          return
        }
        wx.showModal({
          title: '删除题目？',
          content: '删除后无法恢复。',
          confirmText: '删除',
          confirmColor: '#E5484D',
          success: function (m) {
            if (!m.confirm) return
            stores.interview.remove(id)
            self.refresh()
            wx.showToast({ title: '已删除', icon: 'success' })
          }
        })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'DevBench · 面试题库', path: '/pages/tech/interview/index' }
  }
}))
