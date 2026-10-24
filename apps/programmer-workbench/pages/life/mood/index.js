/**
 * 心情
 *
 * 结构：今日打卡（5 个圆点）→ 最近 14 天色带 → 历史记录列表。
 * 点圆点记录今天；点历史项编辑当天；长按删除。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const constants = require('../../../core/constants.js')
const dateUtil = require('../../../core/date.js')

const STRIP_DAYS = 14

Page(withTheme({
  data: {
    title: '心情',
    levels: [],
    todayLevel: 0,
    todayLabel: '未记录',
    todayDate: '',

    strip: [],
    items: [],
    count: 0,
    streak: 0,

    editing: false,
    isEdit: false,
    entryId: '',
    formDate: '',
    formLevel: 3,
    formTags: '',
    formNote: '',
    formLevels: []
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const entries = stores.mood.getAll().slice()
    const today = dateUtil.todayStr()

    const byDate = {}
    entries.forEach(function (e) { byDate[e.date] = e })

    const todayEntry = byDate[today]
    const todayLevel = todayEntry ? todayEntry.level : 0

    // 最近 14 天色带
    const strip = []
    for (let i = STRIP_DAYS - 1; i >= 0; i--) {
      const d = dateUtil.daysAgo(i)
      const e = byDate[d]
      const lv = e ? e.level : 0
      strip.push({
        date: d,
        day: Number(d.slice(8, 10)),
        level: lv,
        color: lv ? constants.moodLevel(lv).color : '',
        isToday: i === 0
      })
    }

    // 历史列表（日期倒序）
    entries.sort(function (a, b) {
      if (a.date !== b.date) return a.date < b.date ? 1 : -1
      return 0
    })

    const items = entries.map(function (e) {
      const lv = constants.moodLevel(e.level)
      return {
        id: e.id,
        date: e.date,
        dateLabel: dateUtil.humanDay(e.date),
        level: e.level,
        levelLabel: lv.label,
        color: lv.color,
        tags: e.tags || [],
        note: e.note || ''
      }
    })

    this.setData({
      levels: constants.MOOD_LEVELS.map(function (m) {
        return {
          level: m.level,
          label: m.label,
          color: m.color,
          active: m.level === todayLevel
        }
      }),
      todayLevel: todayLevel,
      todayLabel: todayLevel ? constants.moodLevel(todayLevel).label : '未记录',
      todayDate: today,
      strip: strip,
      items: items,
      count: items.length,
      streak: this.calcStreak(byDate)
    })
  },

  calcStreak (byDate) {
    const cursor = new Date()
    if (!byDate[dateUtil.todayStr()]) cursor.setDate(cursor.getDate() - 1)

    let streak = 0
    for (let i = 0; i < 3650; i++) {
      const d = dateUtil.formatYMD(cursor)
      if (!byDate[d]) break
      streak += 1
      cursor.setDate(cursor.getDate() - 1)
    }
    return streak
  },

  /** 点圆点记录今天 */
  onSetToday (e) {
    const level = Number(e.currentTarget.dataset.level)
    stores.mood.upsertToday({ level: level })
    this.refresh()
    wx.vibrateShort({ type: 'light' })
    wx.showToast({ title: '已记录：' + constants.moodLevel(level).label, icon: 'none' })
  },

  // ==================== 面板 ====================

  onOpen (e) {
    const entry = stores.mood.find(e.currentTarget.dataset.id)
    if (!entry) return
    this.openPanel(entry)
  },

  openPanel (entry) {
    const level = entry ? entry.level : 3
    this.setData({
      editing: true,
      isEdit: true,
      entryId: entry ? entry.id : '',
      formDate: entry ? entry.date : dateUtil.todayStr(),
      formLevel: level,
      formTags: entry ? (entry.tags || []).join(', ') : '',
      formNote: entry ? (entry.note || '') : '',
      formLevels: this.buildLevels(level)
    })
  },

  onClosePanel () {
    this.setData({ editing: false })
  },

  buildLevels (activeLevel) {
    return constants.MOOD_LEVELS.map(function (m) {
      return {
        level: m.level,
        label: m.label,
        color: m.color,
        active: m.level === activeLevel
      }
    })
  },

  onFormLevel (e) {
    const level = Number(e.currentTarget.dataset.level)
    this.setData({ formLevel: level, formLevels: this.buildLevels(level) })
  },

  onFormTags (e) {
    this.setData({ formTags: e.detail.value })
  },

  onFormNote (e) {
    this.setData({ formNote: e.detail.value })
  },

  onSave () {
    const tags = String(this.data.formTags || '')
      .split(/[,，、\s]+/)
      .map(function (t) { return t.trim().replace(/^#/, '') })
      .filter(function (t) { return !!t })

    const payload = {
      level: this.data.formLevel,
      tags: tags,
      note: String(this.data.formNote || '').trim()
    }

    if (this.data.isEdit) {
      stores.mood.update(this.data.entryId, payload)
    } else {
      stores.mood.upsertToday(payload, this.data.formDate)
    }

    this.setData({ editing: false })
    this.refresh()
    wx.showToast({ title: '已保存', icon: 'success' })
  },

  onDelete () {
    if (!this.data.isEdit) return
    const self = this
    wx.showModal({
      title: '删除这天的记录？',
      content: '删除后无法恢复。',
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.mood.remove(self.data.entryId)
        self.setData({ editing: false })
        self.refresh()
        wx.showToast({ title: '已删除', icon: 'success' })
      }
    })
  },

  onLongPress (e) {
    const id = e.currentTarget.dataset.id
    const entry = stores.mood.find(id)
    if (!entry) return
    const self = this
    wx.showModal({
      title: '删除这天的记录？',
      content: entry.date,
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.mood.remove(id)
        self.refresh()
        wx.showToast({ title: '已删除', icon: 'success' })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'DevBench · 心情', path: '/pages/life/mood/index' }
  }
}))
