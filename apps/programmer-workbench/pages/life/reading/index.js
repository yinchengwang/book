/**
 * 读书
 *
 * 排序：在读 → 想读 → 搁置 → 已读。
 * 点卡片打开编辑面板，可直接改当前页码（读完自动转已读）。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const constants = require('../../../core/constants.js')
const fmt = require('../../../core/format.js')

const STATUS_ORDER = { reading: 0, wishlist: 1, paused: 2, finished: 3 }

Page(withTheme({
  data: {
    title: '读书',
    items: [],
    count: 0,
    readingCount: 0,
    finishedCount: 0,

    editing: false,
    isEdit: false,
    bookId: '',
    formTitle: '',
    formAuthor: '',
    formTotal: '',
    formCurrent: '',
    formStatus: 'reading',
    formRating: 0,
    formNotes: '',
    statuses: [],
    ratingOptions: [1, 2, 3, 4, 5]
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const list = stores.reading.getAll().slice()

    list.sort(function (a, b) {
      const oa = STATUS_ORDER[a.status] === undefined ? 9 : STATUS_ORDER[a.status]
      const ob = STATUS_ORDER[b.status] === undefined ? 9 : STATUS_ORDER[b.status]
      if (oa !== ob) return oa - ob
      return new Date(b.createdAt || 0).getTime() - new Date(a.createdAt || 0).getTime()
    })

    const items = list.map(function (r) {
      const status = constants.readingStatus(r.status)
      const total = r.totalPages || 0
      const current = r.currentPage || 0
      return {
        id: r.id,
        title: r.title,
        author: r.author || '',
        color: status.color,
        statusLabel: status.label,
        status: r.status,
        current: current,
        total: total,
        percent: fmt.percent(current, total),
        rating: r.rating || 0,
        progressText: total > 0 ? current + ' / ' + total + ' 页' : '未设总页数'
      }
    })

    this.setData({
      items: items,
      count: items.length,
      readingCount: items.filter(function (i) { return i.status === 'reading' }).length,
      finishedCount: items.filter(function (i) { return i.status === 'finished' }).length
    })
  },

  // ==================== 面板 ====================

  onCreate () {
    this.openPanel(null)
  },

  onOpen (e) {
    const item = stores.reading.find(e.currentTarget.dataset.id)
    if (item) this.openPanel(item)
  },

  openPanel (item) {
    const isEdit = !!item
    const status = item ? item.status : 'reading'

    this.setData({
      editing: true,
      isEdit: isEdit,
      bookId: isEdit ? item.id : '',
      formTitle: isEdit ? item.title : '',
      formAuthor: isEdit ? (item.author || '') : '',
      formTotal: isEdit && item.totalPages ? String(item.totalPages) : '',
      formCurrent: isEdit ? String(item.currentPage || 0) : '0',
      formStatus: status,
      formRating: isEdit ? (item.rating || 0) : 0,
      formNotes: isEdit ? (item.notes || '') : '',
      statuses: this.buildStatuses(status)
    })
  },

  onClosePanel () {
    this.setData({ editing: false })
  },

  buildStatuses (activeKey) {
    return constants.READING_STATUS.map(function (s) {
      return { key: s.key, label: s.label, color: s.color, active: s.key === activeKey }
    })
  },

  onFormTitle (e) { this.setData({ formTitle: e.detail.value }) },
  onFormAuthor (e) { this.setData({ formAuthor: e.detail.value }) },
  onFormTotal (e) { this.setData({ formTotal: e.detail.value }) },
  onFormCurrent (e) { this.setData({ formCurrent: e.detail.value }) },
  onFormNotes (e) { this.setData({ formNotes: e.detail.value }) },

  onFormStatus (e) {
    const key = e.currentTarget.dataset.key
    this.setData({ formStatus: key, statuses: this.buildStatuses(key) })
  },

  onFormRating (e) {
    this.setData({ formRating: Number(e.currentTarget.dataset.star) })
  },

  onSave () {
    const title = String(this.data.formTitle || '').trim()
    if (!title) {
      wx.showToast({ title: '请输入书名', icon: 'none' })
      return
    }

    const total = Number(this.data.formTotal) || 0
    let current = Number(this.data.formCurrent) || 0
    if (total > 0) current = Math.min(current, total)
    if (current < 0) current = 0

    let status = this.data.formStatus
    // 页数读满自动转已读
    if (total > 0 && current >= total) status = 'finished'

    const payload = {
      title: title,
      author: String(this.data.formAuthor || '').trim(),
      totalPages: total,
      currentPage: current,
      status: status,
      rating: this.data.formRating,
      notes: String(this.data.formNotes || '').trim()
    }

    // 读满即视为完成，无论新建还是编辑都要记完成时间
    if (status === 'finished') payload.finishedAt = new Date().toISOString()

    if (this.data.isEdit) {
      stores.reading.update(this.data.bookId, payload)
    } else {
      stores.reading.add(payload)
    }

    this.setData({ editing: false })
    this.refresh()
    wx.showToast({ title: '已保存', icon: 'success' })
  },

  onDelete () {
    if (!this.data.isEdit) return
    const self = this
    wx.showModal({
      title: '删除这本书？',
      content: '删除后无法恢复。',
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.reading.remove(self.data.bookId)
        self.setData({ editing: false })
        self.refresh()
        wx.showToast({ title: '已删除', icon: 'success' })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'DevBench · 读书', path: '/pages/life/reading/index' }
  }
}))
