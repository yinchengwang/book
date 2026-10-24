/**
 * 日程
 *
 * 排序：未完成在前，其次按开始时间升序。
 * 点卡片编辑，点左侧圆圈切换完成，长按删除。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const dateUtil = require('../../../core/date.js')

Page(withTheme({
  data: {
    title: '日程',
    items: [],
    count: 0,
    todayCount: 0,
    openCount: 0,

    editing: false,
    isEdit: false,
    eventId: '',
    formTitle: '',
    formDate: '',
    formTime: '10:00',
    formLocation: '',
    formNote: '',
    formReminder: true
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const list = stores.schedule.getAll().slice()

    list.sort(function (a, b) {
      if (!!a.done !== !!b.done) return a.done ? 1 : -1
      return String(a.startAt || '').localeCompare(String(b.startAt || ''))
    })

    const today = dateUtil.todayStr()

    const items = list.map(function (e) {
      const start = String(e.startAt || '')
      const datePart = start.slice(0, 10)
      const timePart = start.length >= 16 ? start.slice(11, 16) : ''
      return {
        id: e.id,
        title: e.title,
        done: !!e.done,
        dateLabel: dateUtil.humanDay(datePart),
        timeLabel: timePart,
        location: e.location || '',
        note: e.note || '',
        reminder: !!e.reminder,
        isToday: datePart === today
      }
    })

    this.setData({
      items: items,
      count: items.length,
      todayCount: items.filter(function (i) { return i.isToday && !i.done }).length,
      openCount: items.filter(function (i) { return !i.done }).length
    })
  },

  onToggleDone (e) {
    const id = e.currentTarget.dataset.id
    const item = stores.schedule.find(id)
    if (!item) return
    stores.schedule.update(id, { done: !item.done })
    this.refresh()
    wx.vibrateShort({ type: 'light' })
  },

  // ==================== 面板 ====================

  onCreate () {
    this.openPanel(null)
  },

  onOpen (e) {
    const item = stores.schedule.find(e.currentTarget.dataset.id)
    if (item) this.openPanel(item)
  },

  openPanel (item) {
    const isEdit = !!item
    let date = dateUtil.todayStr()
    let time = '10:00'

    if (isEdit && item.startAt) {
      const s = String(item.startAt)
      date = s.slice(0, 10)
      if (s.length >= 16) time = s.slice(11, 16)
    }

    this.setData({
      editing: true,
      isEdit: isEdit,
      eventId: isEdit ? item.id : '',
      formTitle: isEdit ? item.title : '',
      formDate: date,
      formTime: time,
      formLocation: isEdit ? (item.location || '') : '',
      formNote: isEdit ? (item.note || '') : '',
      formReminder: isEdit ? !!item.reminder : true
    })
  },

  onClosePanel () {
    this.setData({ editing: false })
  },

  onFormTitle (e) { this.setData({ formTitle: e.detail.value }) },
  onFormLocation (e) { this.setData({ formLocation: e.detail.value }) },
  onFormNote (e) { this.setData({ formNote: e.detail.value }) },

  onFormDate (e) {
    this.setData({ formDate: e.detail.value })
  },

  onFormTime (e) {
    this.setData({ formTime: e.detail.value })
  },

  onToggleReminder () {
    this.setData({ formReminder: !this.data.formReminder })
  },

  onSave () {
    const title = String(this.data.formTitle || '').trim()
    if (!title) {
      wx.showToast({ title: '请输入日程标题', icon: 'none' })
      return
    }

    const date = this.data.formDate || dateUtil.todayStr()
    const time = this.data.formTime || '00:00'
    const startAt = date + 'T' + time + ':00'

    const payload = {
      title: title,
      startAt: startAt,
      location: String(this.data.formLocation || '').trim(),
      note: String(this.data.formNote || '').trim(),
      reminder: this.data.formReminder
    }

    if (this.data.isEdit) {
      stores.schedule.update(this.data.eventId, payload)
    } else {
      stores.schedule.add(Object.assign({ done: false }, payload))
    }

    this.setData({ editing: false })
    this.refresh()
    wx.showToast({ title: '已保存', icon: 'success' })
  },

  onDelete () {
    if (!this.data.isEdit) return
    const self = this
    wx.showModal({
      title: '删除这条日程？',
      content: '删除后无法恢复。',
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.schedule.remove(self.data.eventId)
        self.setData({ editing: false })
        self.refresh()
        wx.showToast({ title: '已删除', icon: 'success' })
      }
    })
  },

  onLongPress (e) {
    const id = e.currentTarget.dataset.id
    const item = stores.schedule.find(id)
    if (!item) return
    const self = this
    wx.showModal({
      title: '删除这条日程？',
      content: item.title,
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.schedule.remove(id)
        self.refresh()
        wx.showToast({ title: '已删除', icon: 'success' })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'DevBench · 日程', path: '/pages/life/schedule/index' }
  }
}))
