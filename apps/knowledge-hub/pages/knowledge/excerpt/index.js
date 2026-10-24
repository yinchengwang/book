/**
 * 摘抄管理 —— 搜索 / 按书籍·标签筛选 / 收藏 / 状态流转
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const khc = require('../../../core/kh-constants.js')
const date = require('../../../core/date.js')

Page(withTheme({
  data: {
    statuses: khc.EXCERPT_STATUS,
    query: '',
    filterBook: '',
    filterTag: '',
    books: [],
    tags: [],
    list: [],
    stats: null,
    current: null,
    showAdd: false,
    form: { content: '', book: '', tags: '', note: '' }
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const der = stores.excerpts.derive()
    const all = stores.excerpts.getAll()
    const q = (this.data.query || '').trim().toLowerCase()
    const fb = this.data.filterBook
    const ft = this.data.filterTag

    let list = all
    if (fb) list = list.filter(function (e) { return e.book === fb })
    if (ft) list = list.filter(function (e) { return (e.tags || []).indexOf(ft) >= 0 })
    if (q) list = list.filter(function (e) {
      return (e.content || '').toLowerCase().indexOf(q) >= 0 ||
        (e.book || '').toLowerCase().indexOf(q) >= 0
    })

    const keep = this.data.current ? stores.excerpts.find(this.data.current.id) : null

    this.setData({
      books: der.books,
      tags: der.tags,
      stats: der.stats,
      list: list.map(function (e) {
        const meta = khc.excerptStatus(e.status)
        return {
          id: e.id,
          content: e.content,
          book: e.book,
          note: e.note,
          favorite: e.favorite,
          status: e.status,
          statusLabel: meta.label,
          statusColor: meta.color,
          tags: e.tags || [],
          date: date.formatRelative(e.createdAt)
        }
      }),
      current: keep ? this.toDetail(keep) : null
    })
  },

  toDetail (e) {
    const meta = khc.excerptStatus(e.status)
    return {
      id: e.id,
      content: e.content,
      book: e.book,
      note: e.note,
      favorite: e.favorite,
      status: e.status,
      statusLabel: meta.label,
      statusColor: meta.color,
      tags: e.tags || [],
      date: e.createdAt ? e.createdAt.slice(0, 10) : ''
    }
  },

  onSearch (e) {
    this.setData({ query: e.detail.value })
    this.refresh()
  },

  onPickBook (e) {
    const b = e.currentTarget.dataset.book
    this.setData({ filterBook: this.data.filterBook === b ? '' : b })
    this.refresh()
  },

  onPickTag (e) {
    const t = e.currentTarget.dataset.tag
    this.setData({ filterTag: this.data.filterTag === t ? '' : t })
    this.refresh()
  },

  onOpen (e) {
    const ex = stores.excerpts.find(e.currentTarget.dataset.id)
    if (ex) this.setData({ current: this.toDetail(ex) })
  },

  onClose () {
    this.setData({ current: null })
  },

  onToggleFav (e) {
    const id = (e.currentTarget.dataset.id) || (this.data.current && this.data.current.id)
    if (!id) return
    stores.excerpts.toggleFavorite(id)
    this.refresh()
  },

  onCycleStatus (e) {
    const id = (e.currentTarget.dataset.id) || (this.data.current && this.data.current.id)
    if (!id) return
    const cur = stores.excerpts.find(id)
    const keys = khc.EXCERPT_STATUS.map(function (s) { return s.key })
    const next = keys[(keys.indexOf(cur.status) + 1) % keys.length]
    stores.excerpts.setStatus(id, next)
    this.refresh()
  },

  onCopy () {
    const c = this.data.current
    if (!c) return
    wx.setClipboardData({
      data: c.content,
      success () { wx.showToast({ title: '已复制', icon: 'none' }) }
    })
  },

  onDelete (e) {
    const id = (e.currentTarget.dataset.id) || (this.data.current && this.data.current.id)
    if (!id) return
    wx.showModal({
      title: '删除摘抄',
      content: '确定删除这条摘抄？',
      success: (res) => {
        if (!res.confirm) return
        stores.excerpts.remove(id)
        this.setData({ current: null })
        this.refresh()
      }
    })
  },

  // ---- 新增 ----
  onOpenAdd () {
    this.setData({ showAdd: true, form: { content: '', book: '', tags: '', note: '' } })
  },

  onCloseAdd () {
    this.setData({ showAdd: false })
  },

  onAddInput (e) {
    this.setData({ ['form.' + e.currentTarget.dataset.field]: e.detail.value })
  },

  onSave () {
    const f = this.data.form
    if (!f.content.trim()) {
      wx.showToast({ title: '请填写摘抄内容', icon: 'none' })
      return
    }
    stores.excerpts.addExcerpt({
      content: f.content.trim(),
      book: f.book.trim(),
      tags: f.tags.split(',').map(function (t) { return t.trim() }).filter(Boolean),
      note: f.note.trim(),
      status: 'reading'
    })
    this.setData({ showAdd: false })
    this.refresh()
    wx.showToast({ title: '已添加', icon: 'none' })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 摘抄管理', path: '/pages/knowledge/excerpt/index' }
  }
}))