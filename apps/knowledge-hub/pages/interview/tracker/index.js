/**
 * 面试追踪 —— 公司列表 / 状态流转 / 面试时间线
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const khc = require('../../../core/kh-constants.js')
const date = require('../../../core/date.js')

Page(withTheme({
  data: {
    statuses: khc.INTERVIEW_STATUSES,
    recTypes: khc.INTERVIEW_TYPES,
    list: [],
    current: null,
    showAdd: false,
    showRecord: false,
    form: { name: '', position: '', salary: '', notes: '' },
    recForm: { type: '一面', content: '' }
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const keep = this.data.current ? stores.interviews.find(this.data.current.id) : null
    const list = stores.interviews.getAll().map(function (c) {
      const meta = khc.interviewStatus(c.status)
      return {
        id: c.id,
        name: c.name,
        position: c.position,
        status: c.status,
        statusColor: meta.color,
        progress: meta.progress,
        salary: c.salary,
        notes: c.notes,
        timelineCount: (c.timeline || []).length,
        updated: date.formatRelative(c.createdAt)
      }
    })
    this.setData({ list: list, current: keep })
  },

  onOpen (e) {
    const id = e.currentTarget.dataset.id
    const c = stores.interviews.find(id)
    if (!c) return
    this.setData({
      current: {
        id: c.id,
        name: c.name,
        position: c.position,
        status: c.status,
        statusColor: khc.interviewStatus(c.status).color,
        salary: c.salary,
        notes: c.notes,
        timeline: (c.timeline || []).slice().reverse().map(function (r) {
          const meta = khc.INTERVIEW_TYPES.filter(function (t) { return t.key === r.type })[0]
          return {
            id: r.id,
            type: r.type,
            typeLabel: meta ? meta.label : r.type,
            content: r.content,
            date: r.date
          }
        })
      }
    })
  },

  onCloseDetail () {
    this.setData({ current: null })
  },

  /** 生成 markdown 面经并复制到剪贴板（原生用 wx.setClipboardData） */
  onShareCompany () {
    const c = this.data.current
    if (!c) return
    const full = stores.interviews.find(c.id)
    const lines = ['# ' + full.name + ' · ' + full.position, '']
    ;(full.timeline || []).forEach(function (r) {
      lines.push('## ' + r.type + '（' + r.date + '）')
      lines.push(r.content)
      lines.push('')
    })
    if (full.notes) {
      lines.push('## 备注')
      lines.push(full.notes)
    }
    wx.setClipboardData({
      data: lines.join('\n'),
      success () { wx.showToast({ title: '面经已复制', icon: 'none' }) }
    })
  },

  onChangeStatus (e) {
    const c = this.data.current
    if (!c) return
    stores.interviews.updateStatus(c.id, e.currentTarget.dataset.status)
    this.refresh()
  },

  // ---- 新增公司 ----
  onOpenAdd () {
    this.setData({ showAdd: true, form: { name: '', position: '', salary: '', notes: '' } })
  },

  onCloseAdd () {
    this.setData({ showAdd: false })
  },

  onAddInput (e) {
    this.setData({ ['form.' + e.currentTarget.dataset.field]: e.detail.value })
  },

  onSaveCompany () {
    const f = this.data.form
    if (!f.name.trim()) {
      wx.showToast({ title: '请填写公司名', icon: 'none' })
      return
    }
    stores.interviews.addCompany({
      name: f.name.trim(),
      position: f.position.trim(),
      salary: f.salary.trim(),
      notes: f.notes.trim(),
      status: '投递'
    })
    this.setData({ showAdd: false })
    this.refresh()
    wx.showToast({ title: '已添加', icon: 'none' })
  },

  // ---- 添加面试记录 ----
  onOpenRecord () {
    this.setData({ showRecord: true, recForm: { type: '一面', content: '' } })
  },

  onCloseRecord () {
    this.setData({ showRecord: false })
  },

  onRecInput (e) {
    this.setData({ ['recForm.' + e.currentTarget.dataset.field]: e.detail.value })
  },

  onPickRecType (e) {
    this.setData({ 'recForm.type': e.currentTarget.dataset.type })
  },

  onSaveRecord () {
    const c = this.data.current
    const f = this.data.recForm
    if (!c || !f.content.trim()) {
      wx.showToast({ title: '请填写面试内容', icon: 'none' })
      return
    }
    stores.interviews.addInterviewRecord(c.id, { type: f.type, content: f.content.trim(), date: date.todayStr() })
    this.setData({ showRecord: false })
    this.refresh()
    wx.showToast({ title: '已记录', icon: 'none' })
  },

  onDelete () {
    const c = this.data.current
    if (!c) return
    wx.showModal({
      title: '删除公司',
      content: '将同时删除该公司的全部面试记录',
      success: (res) => {
        if (!res.confirm) return
        stores.interviews.remove(c.id)
        this.setData({ current: null })
        this.refresh()
      }
    })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 面试追踪', path: '/pages/interview/tracker/index' }
  }
}))