/**
 * 复习计划 —— SM-2 间隔重复
 *
 * 三个视图：due（到期）/ all（全部）/ done（已掌握）
 * 答题后按 0-5 评分，交给 core/review.js 的 SM-2 计算下次复习。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')

const TABS = [
  { key: 'due', label: '待复习' },
  { key: 'all', label: '全部' },
  { key: 'done', label: '已掌握' }
]

const QUALITY = [
  { q: 0, label: '完全忘了', color: '#E5484D' },
  { q: 1, label: '很模糊', color: '#F97316' },
  { q: 2, label: '有点印象', color: '#F5A524' },
  { q: 3, label: '想起来了', color: '#EAB308' },
  { q: 4, label: '比较熟练', color: '#84CC16' },
  { q: 5, label: '完全掌握', color: '#30A46C' }
]

Page(withTheme({
  data: {
    tabs: TABS,
    tab: 'due',
    list: [],
    current: null,
    revealed: false,
    stats: { due: 0, all: 0, done: 0 },
    qualities: QUALITY,
    showAdd: false,
    form: { question: '', answer: '', domain: '算法' }
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const all = stores.reviews.getAll()
    const due = stores.reviews.getDue()
    const done = stores.reviews.getDone()

    let list = all
    if (this.data.tab === 'due') list = due
    else if (this.data.tab === 'done') list = done

    const rows = list.map(function (r) {
      return {
        id: r.id,
        question: r.question,
        answer: r.answer,
        domain: r.domain,
        repetitions: r.repetitions,
        interval: r.interval,
        easiness: Math.round(r.easiness * 100) / 100,
        next: r.nextReviewDate ? r.nextReviewDate.slice(0, 10) : ''
      }
    })

    this.setData({
      list: rows,
      current: rows.length ? rows[0] : null,
      revealed: false,
      stats: { due: due.length, all: all.length, done: done.length }
    })
  },

  onSwitchTab (e) {
    this.setData({ tab: e.currentTarget.dataset.key })
    this.refresh()
  },

  onReveal () {
    this.setData({ revealed: true })
  },

  onRate (e) {
    const q = Number(e.currentTarget.dataset.q)
    const cur = this.data.current
    if (!cur) return
    stores.reviews.rateReview(cur.id, q)
    stores.daily.recordReview(1)
    this.refresh()
    wx.vibrateShort({ type: 'light' })
  },

  onNextCard () {
    const list = this.data.list
    if (!list.length) return
    const idx = this.data.list.findIndex(function (i) { return i.id === this.data.current.id }, this)
    const next = list[(idx + 1) % list.length]
    this.setData({ current: next, revealed: false })
  },

  // ---- 新增卡片 ----
  onOpenAdd () {
    this.setData({ showAdd: true, form: { question: '', answer: '', domain: '算法' } })
  },

  onCloseAdd () {
    this.setData({ showAdd: false })
  },

  onInput (e) {
    const field = e.currentTarget.dataset.field
    const value = e.detail.value
    this.setData({ ['form.' + field]: value })
  },

  onSaveCard () {
    const f = this.data.form
    if (!f.question.trim() || !f.answer.trim()) {
      wx.showToast({ title: '请填写问题与答案', icon: 'none' })
      return
    }
    stores.reviews.addRecord({ question: f.question.trim(), answer: f.answer.trim(), domain: f.domain || '通用' })
    this.setData({ showAdd: false })
    this.refresh()
    wx.showToast({ title: '已加入复习队列', icon: 'none' })
  },

  onDelete (e) {
    const id = e.currentTarget.dataset.id
    wx.showModal({
      title: '删除卡片',
      content: '确定从复习队列中移除？',
      success: (res) => {
        if (!res.confirm) return
        stores.reviews.remove(id)
        this.refresh()
      }
    })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 复习计划', path: '/pages/learn/review/index' }
  }
}))