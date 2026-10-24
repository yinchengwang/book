/**
 * 我的 —— 资料、外观、数据管理
 *
 * 外观：主题三态（浅色/深色/跟随系统）+ 主色 5 选，改完立即生效。
 * 数据：统计 / 导出到剪贴板 / 从剪贴板恢复（merge 或 overwrite）/ 清空。
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')
const storage = require('../../core/storage.js')
const constants = require('../../core/constants.js')

Page(withTheme({
  data: {
    title: '我的',

    profile: {},
    initial: 'D',
    editing: false,
    editForm: {},

    themes: [],
    colors: [],
    haptics: true,

    stats: [],
    usageText: '',
    version: '1.0.0'
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const settings = stores.settings.get()
    const profile = settings.profile || {}
    const nickname = profile.nickname || 'DevBencHer'

    this.setData({
      profile: profile,
      initial: nickname.charAt(0).toUpperCase(),
      themes: constants.THEME_MODES.map(function (t) {
        return { key: t.key, label: t.label, active: t.key === settings.theme }
      }),
      colors: constants.PRIMARY_COLORS.map(function (c) {
        return {
          key: c.key,
          label: c.label,
          swatch: settings.theme === 'dark' ? c.dark : c.light,
          active: c.key === settings.primaryColor
        }
      }),
      haptics: settings.enableHaptics !== false,
      stats: this.buildStats(),
      usageText: this.buildUsage()
    })
  },

  buildStats () {
    return [
      { label: '待办', value: String(stores.todos.getAll().length) },
      { label: '笔记', value: String(stores.techNotes.getAll().length) },
      { label: '片段', value: String(stores.snippets.getAll().length) },
      { label: '面试题', value: String(stores.interview.getAll().length) },
      { label: '资源', value: String(stores.resources.getAll().length) },
      { label: '读书', value: String(stores.reading.getAll().length) },
      { label: '健身', value: String(stores.fitness.getAll().length) },
      { label: '心情', value: String(stores.mood.getAll().length) },
      { label: '日程', value: String(stores.schedule.getAll().length) },
      { label: '项目', value: String(stores.projects.getAll().length) },
      { label: '闪念', value: String(stores.insights.getAll().length) }
    ]
  },

  buildUsage () {
    const u = storage.usage()
    return u.currentSize + ' KB · ' + u.keys + ' 个键'
  },

  // ==================== 资料编辑 ====================

  onToggleEdit () {
    if (this.data.editing) {
      this.setData({ editing: false })
      return
    }
    const p = this.data.profile
    this.setData({
      editing: true,
      editForm: {
        nickname: p.nickname || '',
        role: p.role || '',
        motto: p.motto || '',
        bio: p.bio || ''
      }
    })
  },

  onEditField (e) {
    const field = e.currentTarget.dataset.field
    const value = e.detail.value
    const patch = {}
    patch['editForm.' + field] = value
    this.setData(patch)
  },

  onSaveProfile () {
    const form = this.data.editForm
    const nickname = (form.nickname || '').trim()
    if (!nickname) {
      wx.showToast({ title: '昵称不能为空', icon: 'none' })
      return
    }

    stores.settings.patch({
      profile: Object.assign({}, this.data.profile, {
        nickname: nickname,
        role: (form.role || '').trim(),
        motto: (form.motto || '').trim(),
        bio: (form.bio || '').trim()
      })
    })

    this.setData({ editing: false })
    this.refresh()
    wx.showToast({ title: '已保存', icon: 'success' })
  },

  // ==================== 外观 ====================

  onSetTheme (e) {
    const key = e.currentTarget.dataset.key
    stores.settings.patch({ theme: key })
    this.refreshTheme()
    this.refresh()
  },

  onSetPrimary (e) {
    const key = e.currentTarget.dataset.key
    stores.settings.patch({ primaryColor: key })
    this.refreshTheme()
    this.refresh()
  },

  onToggleHaptics (e) {
    const value = e.detail.value
    stores.settings.patch({ enableHaptics: value })
    this.setData({ haptics: value })
  },

  // ==================== 数据管理 ====================

  onExport () {
    const json = storage.exportAll()
    wx.setClipboardData({
      data: json,
      success: function () {
        wx.showModal({
          title: '已复制到剪贴板',
          content: '共 ' + json.length + ' 字符。请粘贴到任意地方保存，恢复时再复制回来。',
          showCancel: false,
          confirmText: '知道了'
        })
      }
    })
  },

  onImport () {
    const self = this
    wx.getClipboardData({
      success: function (res) {
        const text = (res.data || '').trim()
        if (!text) {
          wx.showToast({ title: '剪贴板为空', icon: 'none' })
          return
        }
        if (text.indexOf('"data"') < 0) {
          wx.showToast({ title: '剪贴板内容不是备份数据', icon: 'none' })
          return
        }

        wx.showActionSheet({
          itemList: ['合并（只补充缺失项）', '覆盖（替换全部数据）'],
          success: function (sheet) {
            const mode = sheet.tapIndex === 1 ? 'overwrite' : 'merge'
            if (mode === 'overwrite') {
              wx.showModal({
                title: '确认覆盖？',
                content: '现有数据将被完全替换，且不可撤销。',
                confirmText: '覆盖',
                confirmColor: '#E5484D',
                success: function (m) {
                  if (m.confirm) self.doImport(text, mode)
                }
              })
              return
            }
            self.doImport(text, mode)
          }
        })
      },
      fail: function () {
        wx.showToast({ title: '读取剪贴板失败', icon: 'none' })
      }
    })
  },

  doImport (text, mode) {
    const result = storage.importAll(text, mode)
    if (!result.ok) {
      wx.showModal({
        title: '恢复失败',
        content: result.error || '备份数据格式不正确',
        showCancel: false
      })
      return
    }
    this.refresh()
    wx.showToast({ title: '已恢复 ' + result.count + ' 项', icon: 'success' })
  },

  onClearData () {
    const self = this
    wx.showModal({
      title: '清空全部数据',
      content: '待办、笔记、片段、心情等全部内容与设置都将删除，且不可撤销。',
      confirmText: '清空',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        storage.clearAll()
        // 重新写入种子数据与默认设置
        stores.settings.reset()
        stores.todos.getAll()
        stores.techNotes.getAll()
        stores.snippets.getAll()
        stores.interview.getAll()
        stores.resources.getAll()
        stores.reading.getAll()
        stores.fitness.getAll()
        stores.mood.getAll()
        stores.schedule.getAll()
        stores.projects.getAll()
        stores.insights.getAll()

        self.refreshTheme()
        self.refresh()
        wx.showToast({ title: '已清空', icon: 'success' })
      }
    })
  },

  // ==================== 导航 ====================

  onGo (e) {
    const path = e.currentTarget.dataset.path
    if (path) wx.navigateTo({ url: path })
  },

  onAbout () {
    wx.showModal({
      title: 'DevBench',
      content: '程序员个人工作台 · 原生小程序\n版本 ' + this.data.version + '\n\n数据全部存储在本机，不上传任何服务器。',
      showCancel: false,
      confirmText: '知道了'
    })
  },

  onShareAppMessage () {
    return { title: 'DevBench · 程序员个人工作台', path: '/pages/dashboard/index' }
  }
}))
