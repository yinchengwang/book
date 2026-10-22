/**
 * 技术笔记 · 详情 / 编辑
 *
 * 带 id 为编辑，不带为新建。内容支持 Markdown，预览用 rich-text。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const constants = require('../../../core/constants.js')
const md = require('../../../core/md.js')

Page(withTheme({
  data: {
    title: '笔记',
    isEdit: false,
    noteId: '',

    formTitle: '',
    content: '',
    category: 'frontend',
    tagsText: '',
    categories: [],

    preview: false,
    html: '',
    saving: false
  },

  onLoad (query) {
    const id = (query && query.id) || ''
    const isEdit = !!id

    let note = null
    if (isEdit) note = stores.techNotes.find(id)

    this.setData({
      isEdit: isEdit,
      noteId: id,
      formTitle: note ? note.title : '',
      content: note ? note.content : '## 新笔记\n\n在这里书写你的想法…',
      category: note ? note.category : 'frontend',
      tagsText: note ? (note.tags || []).join(', ') : '',
      categories: this.buildCategories(note ? note.category : 'frontend')
    })

    wx.setNavigationBarTitle({ title: isEdit ? '编辑笔记' : '新建笔记' })
  },

  buildCategories (activeKey) {
    return constants.TECH_CATEGORIES.map(function (c) {
      return {
        key: c.key,
        label: c.label,
        color: c.color,
        active: c.key === activeKey
      }
    })
  },

  onTitle (e) {
    this.setData({ formTitle: e.detail.value })
  },

  onContent (e) {
    this.setData({ content: e.detail.value })
    if (this.data.preview) this.renderPreview()
  },

  onTags (e) {
    this.setData({ tagsText: e.detail.value })
  },

  onCategory (e) {
    const key = e.currentTarget.dataset.key
    this.setData({
      category: key,
      categories: this.buildCategories(key)
    })
  },

  onTogglePreview () {
    const next = !this.data.preview
    this.setData({ preview: next })
    if (next) this.renderPreview()
  },

  renderPreview () {
    this.setData({ html: md.render(this.data.content, this.data.isDark) })
  },

  /** 解析标签：支持逗号、顿号、空格分隔，自动去掉 # 前缀 */
  parseTags (text) {
    return String(text || '')
      .split(/[,，、\s]+/)
      .map(function (t) { return t.trim().replace(/^#/, '') })
      .filter(function (t) { return !!t })
  },

  onSave () {
    if (this.data.saving) return

    const title = String(this.data.formTitle || '').trim()
    const content = String(this.data.content || '').trim()

    if (!title) {
      wx.showToast({ title: '请输入标题', icon: 'none' })
      return
    }
    if (!content) {
      wx.showToast({ title: '请输入内容', icon: 'none' })
      return
    }

    this.setData({ saving: true })

    const tags = this.parseTags(this.data.tagsText)
    const payload = {
      title: title,
      content: content,
      category: this.data.category,
      tags: tags,
      updatedAt: new Date().toISOString()
    }

    if (this.data.isEdit) {
      stores.techNotes.update(this.data.noteId, payload)
    } else {
      stores.techNotes.add(Object.assign({ pinned: false }, payload))
    }

    wx.showToast({ title: '已保存', icon: 'success' })
    setTimeout(function () { wx.navigateBack() }, 400)
  },

  onDelete () {
    if (!this.data.isEdit) return
    const self = this
    wx.showModal({
      title: '删除笔记？',
      content: '删除后无法恢复。',
      confirmText: '删除',
      confirmColor: '#E5484D',
      success: function (res) {
        if (!res.confirm) return
        stores.techNotes.remove(self.data.noteId)
        wx.showToast({ title: '已删除', icon: 'success' })
        setTimeout(function () { wx.navigateBack() }, 300)
      }
    })
  },

  onShareAppMessage () {
    return { title: this.data.formTitle || '技术笔记', path: '/pages/tech/notes/index' }
  }
}))
