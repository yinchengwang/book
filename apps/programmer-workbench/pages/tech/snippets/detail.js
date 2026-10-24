/**
 * 代码片段 · 详情 / 编辑
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const constants = require('../../../core/constants.js')

Page(withTheme({
  data: {
    title: '片段',
    isEdit: false,
    snippetId: '',

    formTitle: '',
    language: 'typescript',
    description: '',
    code: '',
    tagsText: '',
    favorite: false,
    languages: [],
    lineCount: 1
  },

  onLoad (query) {
    const id = (query && query.id) || ''
    const isEdit = !!id
    const item = isEdit ? stores.snippets.find(id) : null

    this.setData({
      isEdit: isEdit,
      snippetId: id,
      formTitle: item ? item.title : '',
      language: item ? item.language : 'typescript',
      description: item ? (item.description || '') : '',
      code: item ? item.code : '',
      tagsText: item ? (item.tags || []).join(', ') : '',
      favorite: item ? !!item.favorite : false,
      languages: this.buildLanguages(item ? item.language : 'typescript'),
      lineCount: this.countLines(item ? item.code : '')
    })

    wx.setNavigationBarTitle({ title: isEdit ? '编辑片段' : '新建片段' })
  },

  buildLanguages (activeKey) {
    return constants.LANGUAGES.map(function (l) {
      return {
        key: l.key,
        label: l.label,
        color: l.color,
        active: l.key === activeKey
      }
    })
  },

  countLines (code) {
    return String(code || '').split('\n').length
  },

  onTitle (e) {
    this.setData({ formTitle: e.detail.value })
  },

  onDescription (e) {
    this.setData({ description: e.detail.value })
  },

  onCode (e) {
    const code = e.detail.value
    this.setData({ code: code, lineCount: this.countLines(code) })
  },

  onTags (e) {
    this.setData({ tagsText: e.detail.value })
  },

  onLanguage (e) {
    const key = e.currentTarget.dataset.key
    this.setData({
      language: key,
      languages: this.buildLanguages(key)
    })
  },

  onToggleFavorite () {
    this.setData({ favorite: !this.data.favorite })
  },

  onCopyCode () {
    const code = this.data.code
    if (!code) {
      wx.showToast({ title: '没有可复制的代码', icon: 'none' })
      return
    }
    wx.setClipboardData({
      data: code,
      success: function () {
        wx.showToast({ title: '已复制', icon: 'success' })
      }
    })
  },

  parseTags (text) {
    return String(text || '')
      .split(/[,，、\s]+/)
      .map(function (t) { return t.trim().replace(/^#/, '') })
      .filter(function (t) { return !!t })
  },

  onSave () {
    const title = String(this.data.formTitle || '').trim()
    const code = String(this.data.code || '')

    if (!title) {
      wx.showToast({ title: '请输入标题', icon: 'none' })
      return
    }
    if (!code.trim()) {
      wx.showToast({ title: '请输入代码', icon: 'none' })
      return
    }

    const payload = {
      title: title,
      language: this.data.language,
      description: String(this.data.description || '').trim(),
      code: code,
      tags: this.parseTags(this.data.tagsText),
      favorite: this.data.favorite
    }

    if (this.data.isEdit) {
      stores.snippets.update(this.data.snippetId, payload)
    } else {
      stores.snippets.add(payload)
    }

    wx.showToast({ title: '已保存', icon: 'success' })
    setTimeout(function () { wx.navigateBack() }, 400)
  },

  onDelete () {
    if (!this.data.isEdit) return
    const self = this
    wx.showModal({
      title: '删除片段？',
      content: '删除后无法恢复。',
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.snippets.remove(self.data.snippetId)
        wx.showToast({ title: '已删除', icon: 'success' })
        setTimeout(function () { wx.navigateBack() }, 300)
      }
    })
  },

  onShareAppMessage () {
    return { title: this.data.formTitle || '代码片段', path: '/pages/tech/snippets/index' }
  }
}))
