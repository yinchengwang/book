/**
 * 闪念
 *
 * 顶部一行快速输入（回车即存），下方时间线。
 * 排序：置顶优先 → 未归档优先 → 创建时间倒序。
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')
const dateUtil = require('../../core/date.js')

Page(withTheme({
  data: {
    title: '闪念',
    quickText: '',
    items: [],
    count: 0,
    pinnedCount: 0,

    editing: false,
    insightId: '',
    formContent: '',
    formTags: '',
    formPinned: false,
    formArchived: false
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const list = stores.insights.getAll().slice()

    list.sort(function (a, b) {
      if (!!a.pinned !== !!b.pinned) return a.pinned ? -1 : 1
      if (!!a.archived !== !!b.archived) return a.archived ? 1 : -1
      return new Date(b.createdAt || 0).getTime() - new Date(a.createdAt || 0).getTime()
    })

    const items = list.map(function (i) {
      return {
        id: i.id,
        content: i.content,
        tags: i.tags || [],
        pinned: !!i.pinned,
        archived: !!i.archived,
        time: dateUtil.formatRelative(i.createdAt)
      }
    })

    this.setData({
      items: items,
      count: items.length,
      pinnedCount: items.filter(function (i) { return i.pinned }).length
    })
  },

  // ==================== 快速输入 ====================

  onQuickInput (e) {
    this.setData({ quickText: e.detail.value })
  },

  onQuickAdd () {
    const text = String(this.data.quickText || '').trim()
    if (!text) {
      wx.showToast({ title: '写点什么', icon: 'none' })
      return
    }
    stores.insights.add({ content: text, tags: [], pinned: false, archived: false })
    this.setData({ quickText: '' })
    this.refresh()
    wx.showToast({ title: '已记下', icon: 'success' })
  },

  // ==================== 长按操作 ====================

  onLongPress (e) {
    const id = e.currentTarget.dataset.id
    const item = stores.insights.find(id)
    if (!item) return

    const self = this
    wx.showActionSheet({
      itemList: [
        item.pinned ? '取消置顶' : '置顶',
        item.archived ? '取消归档' : '归档',
        '删除'
      ],
      success: function (res) {
        if (res.tapIndex === 0) {
          stores.insights.update(id, { pinned: !item.pinned })
          self.refresh()
          return
        }
        if (res.tapIndex === 1) {
          stores.insights.update(id, { archived: !item.archived })
          self.refresh()
          return
        }
        wx.showModal({
          title: '删除这条闪念？',
          content: '删除后无法恢复。',
          confirmText: '删除',
          confirmColor: '#E5484D',
          success: function (m) {
            if (!m.confirm) return
            stores.insights.remove(id)
            self.refresh()
            wx.showToast({ title: '已删除', icon: 'success' })
          }
        })
      }
    })
  },

  // ==================== 编辑面板 ====================

  onOpen (e) {
    const item = stores.insights.find(e.currentTarget.dataset.id)
    if (!item) return
    this.setData({
      editing: true,
      insightId: item.id,
      formContent: item.content,
      formTags: (item.tags || []).join(', '),
      formPinned: !!item.pinned,
      formArchived: !!item.archived
    })
  },

  onClosePanel () {
    this.setData({ editing: false })
  },

  onFormContent (e) { this.setData({ formContent: e.detail.value }) },
  onFormTags (e) { this.setData({ formTags: e.detail.value }) },

  onTogglePinned () {
    this.setData({ formPinned: !this.data.formPinned })
  },

  onToggleArchived () {
    this.setData({ formArchived: !this.data.formArchived })
  },

  onSave () {
    const content = String(this.data.formContent || '').trim()
    if (!content) {
      wx.showToast({ title: '内容不能为空', icon: 'none' })
      return
    }

    const tags = String(this.data.formTags || '')
      .split(/[,，、\s]+/)
      .map(function (t) { return t.trim().replace(/^#/, '') })
      .filter(function (t) { return !!t })

    stores.insights.update(this.data.insightId, {
      content: content,
      tags: tags,
      pinned: this.data.formPinned,
      archived: this.data.formArchived
    })

    this.setData({ editing: false })
    this.refresh()
    wx.showToast({ title: '已保存', icon: 'success' })
  },

  onDelete () {
    const self = this
    wx.showModal({
      title: '删除这条闪念？',
      content: '删除后无法恢复。',
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.insights.remove(self.data.insightId)
        self.setData({ editing: false })
        self.refresh()
        wx.showToast({ title: '已删除', icon: 'success' })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'DevBench · 闪念', path: '/pages/insights/index' }
  }
}))
