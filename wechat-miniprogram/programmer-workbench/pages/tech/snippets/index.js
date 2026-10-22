/**
 * 代码片段 · 列表
 *
 * 排序：收藏优先，其次按创建时间倒序。
 * 长按卡片可收藏 / 取消收藏 / 删除。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const constants = require('../../../core/constants.js')
const dateUtil = require('../../../core/date.js')

Page(withTheme({
  data: {
    title: '代码片段',
    items: [],
    count: 0,
    favoriteCount: 0
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const list = stores.snippets.getAll().slice()

    list.sort(function (a, b) {
      if (!!a.favorite !== !!b.favorite) return a.favorite ? -1 : 1
      return new Date(b.createdAt || 0).getTime() - new Date(a.createdAt || 0).getTime()
    })

    const items = list.map(function (s) {
      const lang = constants.language(s.language)
      const lineCount = String(s.code || '').split('\n').length
      return {
        id: s.id,
        title: s.title,
        color: lang.color,
        languageLabel: lang.label,
        desc: s.description || '',
        time: dateUtil.formatRelative(s.createdAt),
        lineCount: lineCount,
        tags: (s.tags || []).slice(0, 3),
        favorite: !!s.favorite
      }
    })

    this.setData({
      items: items,
      count: items.length,
      favoriteCount: items.filter(function (i) { return i.favorite }).length
    })
  },

  onOpen (e) {
    wx.navigateTo({ url: '/pages/tech/snippets/detail?id=' + e.currentTarget.dataset.id })
  },

  onCreate () {
    wx.navigateTo({ url: '/pages/tech/snippets/detail' })
  },

  onLongPress (e) {
    const id = e.currentTarget.dataset.id
    const item = stores.snippets.find(id)
    if (!item) return

    const self = this
    wx.showActionSheet({
      itemList: [item.favorite ? '取消收藏' : '收藏', '删除'],
      success: function (res) {
        if (res.tapIndex === 0) {
          stores.snippets.update(id, { favorite: !item.favorite })
          self.refresh()
          wx.showToast({ title: item.favorite ? '已取消收藏' : '已收藏', icon: 'none' })
          return
        }
        wx.showModal({
          title: '删除片段？',
          content: '「' + item.title + '」删除后无法恢复。',
          confirmText: '删除',
          confirmColor: '#E5484D',
          success: function (m) {
            if (!m.confirm) return
            stores.snippets.remove(id)
            self.refresh()
            wx.showToast({ title: '已删除', icon: 'success' })
          }
        })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'DevBench · 代码片段', path: '/pages/tech/snippets/index' }
  }
}))
