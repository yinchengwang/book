/**
 * 健身
 *
 * 顶部汇总本周次数 / 时长 / 消耗，下方按日期倒序列出记录。
 * 点卡片编辑，长按删除。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const constants = require('../../../core/constants.js')
const dateUtil = require('../../../core/date.js')
const fmt = require('../../../core/format.js')

Page(withTheme({
  data: {
    title: '健身',
    items: [],
    count: 0,
    weekCount: 0,
    weekMinutes: 0,
    weekCalories: 0,

    editing: false,
    isEdit: false,
    recordId: '',
    formDate: '',
    formType: 'cardio',
    formName: '',
    formDuration: '',
    formCalories: '',
    formNotes: '',
    types: []
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const list = stores.fitness.getAll().slice()

    list.sort(function (a, b) {
      if (a.date !== b.date) return a.date < b.date ? 1 : -1
      return new Date(b.createdAt || 0).getTime() - new Date(a.createdAt || 0).getTime()
    })

    const week = dateUtil.weekRange()
    let weekCount = 0
    let weekMinutes = 0
    let weekCalories = 0

    list.forEach(function (r) {
      if (r.date >= week.start && r.date <= week.end) {
        weekCount += 1
        weekMinutes += (r.durationMin || 0)
        weekCalories += (r.calories || 0)
      }
    })

    const items = list.map(function (r) {
      const type = constants.fitnessType(r.type)
      return {
        id: r.id,
        name: r.name,
        date: r.date,
        dateLabel: dateUtil.humanDay(r.date),
        typeLabel: type.label,
        duration: fmt.duration(r.durationMin || 0),
        calories: r.calories || 0,
        notes: r.notes || ''
      }
    })

    this.setData({
      items: items,
      count: items.length,
      weekCount: weekCount,
      weekMinutes: weekMinutes,
      weekCalories: weekCalories
    })
  },

  // ==================== 面板 ====================

  onCreate () {
    this.openPanel(null)
  },

  onOpen (e) {
    const item = stores.fitness.find(e.currentTarget.dataset.id)
    if (item) this.openPanel(item)
  },

  openPanel (item) {
    const isEdit = !!item
    const type = item ? item.type : 'cardio'

    this.setData({
      editing: true,
      isEdit: isEdit,
      recordId: isEdit ? item.id : '',
      formDate: isEdit ? item.date : dateUtil.todayStr(),
      formType: type,
      formName: isEdit ? item.name : '',
      formDuration: isEdit ? String(item.durationMin || '') : '',
      formCalories: isEdit && item.calories ? String(item.calories) : '',
      formNotes: isEdit ? (item.notes || '') : '',
      types: this.buildTypes(type)
    })
  },

  onClosePanel () {
    this.setData({ editing: false })
  },

  buildTypes (activeKey) {
    return constants.FITNESS_TYPES.map(function (t) {
      return { key: t.key, label: t.label, active: t.key === activeKey }
    })
  },

  onFormDate (e) {
    this.setData({ formDate: e.detail.value })
  },

  onFormType (e) {
    const key = e.currentTarget.dataset.key
    this.setData({ formType: key, types: this.buildTypes(key) })
  },

  onFormName (e) { this.setData({ formName: e.detail.value }) },
  onFormDuration (e) { this.setData({ formDuration: e.detail.value }) },
  onFormCalories (e) { this.setData({ formCalories: e.detail.value }) },
  onFormNotes (e) { this.setData({ formNotes: e.detail.value }) },

  onSave () {
    const name = String(this.data.formName || '').trim()
    const duration = Number(this.data.formDuration) || 0

    if (!name) {
      wx.showToast({ title: '请输入运动名称', icon: 'none' })
      return
    }
    if (duration <= 0) {
      wx.showToast({ title: '请输入运动时长', icon: 'none' })
      return
    }

    const payload = {
      date: this.data.formDate || dateUtil.todayStr(),
      type: this.data.formType,
      name: name,
      durationMin: duration,
      calories: Number(this.data.formCalories) || 0,
      notes: String(this.data.formNotes || '').trim()
    }

    if (this.data.isEdit) {
      stores.fitness.update(this.data.recordId, payload)
    } else {
      stores.fitness.add(payload)
    }

    this.setData({ editing: false })
    this.refresh()
    wx.showToast({ title: '已保存', icon: 'success' })
  },

  onDelete () {
    if (!this.data.isEdit) return
    const self = this
    wx.showModal({
      title: '删除这条记录？',
      content: '删除后无法恢复。',
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.fitness.remove(self.data.recordId)
        self.setData({ editing: false })
        self.refresh()
        wx.showToast({ title: '已删除', icon: 'success' })
      }
    })
  },

  onLongPress (e) {
    const id = e.currentTarget.dataset.id
    const item = stores.fitness.find(id)
    if (!item) return
    const self = this
    wx.showModal({
      title: '删除这条记录？',
      content: item.date + ' · ' + item.name,
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.fitness.remove(id)
        self.refresh()
        wx.showToast({ title: '已删除', icon: 'success' })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'DevBench · 健身', path: '/pages/life/fitness/index' }
  }
}))
