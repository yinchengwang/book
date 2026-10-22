/**
 * 我的 —— 资料 / 主题 / 数据管理
 *
 * 原 settings 页的导出/导入/清空用了 localStorage + Blob + document，
 * 小程序不可用，改为 storage.js 的 exportAll / importAll / clearAll
 * + wx.setClipboardData / wx.getFileSystemManager。
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')
const storage = require('../../core/storage.js')
const theme = require('../../core/theme.js')
const date = require('../../core/date.js')

const THEME_MODES = require('../../core/constants.js').THEME_MODES
const PRIMARY_COLORS = require('../../core/constants.js').PRIMARY_COLORS

Page(withTheme({
  data: {
    user: null,
    settings: null,
    themeModes: THEME_MODES,
    primaryColors: PRIMARY_COLORS,
    usage: { keys: 0, currentSize: 0 },
    showEdit: false,
    form: { name: '', bio: '', email: '' },
    importText: ''
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const user = stores.user.get()
    const settings = stores.settings.get()
    this.setData({
      user: user,
      settings: settings,
      usage: storage.usage()
    })
  },

  /* ---- 资料 ---- */
  onOpenEdit () {
    const u = this.data.user
    this.setData({ showEdit: true, form: { name: u.name || '', bio: u.bio || '', email: u.email || '' } })
  },

  onCloseEdit () {
    this.setData({ showEdit: false })
  },

  onEditInput (e) {
    this.setData({ ['form.' + e.currentTarget.dataset.field]: e.detail.value })
  },

  onSaveProfile () {
    const f = this.data.form
    if (f.name.trim()) stores.user.setName(f.name.trim())
    if (f.email.trim()) stores.user.setEmail(f.email.trim())
    stores.user.setBio(f.bio.trim())
    this.setData({ showEdit: false })
    this.refresh()
    wx.showToast({ title: '已保存', icon: 'none' })
  },

  /* ---- 主题 ---- */
  onPickTheme (e) {
    stores.settings.patch({ theme: e.currentTarget.dataset.key })
    this.refresh()
    this.refreshTheme()
  },

  onPickPrimary (e) {
    stores.settings.patch({ primaryColor: e.currentTarget.dataset.key })
    this.refresh()
    this.refreshTheme()
  },

  /* ---- 数据 ---- */
  onExport () {
    const json = storage.exportAll()
    wx.setClipboardData({
      data: json,
      success () { wx.showToast({ title: '备份已复制到剪贴板', icon: 'none' }) }
    })
  },

  onImportInput (e) {
    this.setData({ importText: e.detail.value })
  },

  onImport () {
    const text = this.data.importText
    if (!text.trim()) {
      wx.showToast({ title: '请粘贴备份 JSON', icon: 'none' })
      return
    }
    const res = storage.importAll(text, 'merge')
    if (!res.ok) {
      wx.showToast({ title: res.error || '导入失败', icon: 'none' })
      return
    }
    this.setData({ importText: '' })
    this.refresh()
    wx.showToast({ title: '已导入 ' + res.count + ' 项', icon: 'none' })
  },

  onClear () {
    wx.showModal({
      title: '清空数据',
      content: '将删除本应用全部本地数据，不可恢复',
      success: (res) => {
        if (!res.confirm) return
        storage.clearAll()
        this.refresh()
        wx.showToast({ title: '已清空', icon: 'none' })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar', path: '/pages/profile/index' }
  }
}))