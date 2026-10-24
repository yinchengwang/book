/**
 * 技术笔记 · 列表
 *
 * 排序：置顶优先，其次按更新时间倒序。
 * 长按卡片可置顶 / 删除。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const constants = require('../../../core/constants.js')
const dateUtil = require('../../../core/date.js')
const md = require('../../../core/md.js')

Page(withTheme({
  data: {
    title: '技术笔记',
    items: [],
    count: 0,
    pinnedCount: 0
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const list = stores.techNotes.getAll().slice()

    list.sort(function (a, b) {
      if (!!a.pinned !== !!b.pinned) return a.pinned ? -1 : 1
      const ta = new Date(a.updatedAt || a.createdAt || 0).getTime()
      const tb = new Date(b.updatedAt || b.createdAt || 0).getTime()
      return tb - ta
    })

    const items = list.map(function (n) {
      const cat = constants.techCategory(n.category)
      return {
        id: n.id,
        title: n.title,
        color: cat.color,
        categoryLabel: cat.label,
        time: dateUtil.formatRelative(n.updatedAt || n.createdAt),
        preview: md.preview(n.content, 60),
        tags: (n.tags || []).slice(0, 3),
        pinned: !!n.pinned
      }
    })

    this.setData({
      items: items,
      count: items.length,
      pinnedCount: items.filter(function (i) { return i.pinned }).length
    })
  },

  onOpen (e) {
    wx.navigateTo({ url: '/pages/tech/notes/detail?id=' + e.currentTarget.dataset.id })
  },

  onCreate () {
    wx.navigateTo({ url: '/pages/tech/notes/detail' })
  },

  /** 长按：置顶 / 取消置顶 / 删除 */
  onLongPress (e) {
    const id = e.currentTarget.dataset.id
    const note = stores.techNotes.find(id)
    if (!note) return

    const self = this
    const actions = [note.pinned ? '取消置顶' : '置顶', '删除']

    wx.showActionSheet({
      itemList: actions,
      itemColor: '#0F172A',
      success: function (res) {
        if (res.tapIndex === 0) {
          stores.techNotes.update(id, { pinned: !note.pinned })
          self.refresh()
          wx.showToast({ title: note.pinned ? '已取消置顶' : '已置顶', icon: 'none' })
          return
        }
        wx.showModal({
          title: '删除笔记？',
          content: '「' + note.title + '」删除后无法恢复。',
          confirmText: '删除',
          confirmColor: '#E5484D',
          success: function (m) {
            if (!m.confirm) return
            stores.techNotes.remove(id)
            self.refresh()
            wx.showToast({ title: '已删除', icon: 'success' })
          }
        })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'DevBench · 技术笔记', path: '/pages/tech/notes/index' }
  }
}))
