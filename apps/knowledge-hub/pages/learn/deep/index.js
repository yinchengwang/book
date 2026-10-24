/**
 * 图解系列 —— Markdown 长文阅读器（复用 core/md.js）
 *
 * 列表：按栈筛选 + 搜索；详情：rich-text 渲染。
 * 沿用 PWB 的「无详情页模块用全屏 panel 就地阅读」范式，避免增加页面数。
 */

const { withTheme } = require('../../../core/with-theme.js')
const khc = require('../../../core/kh-constants.js')
const md = require('../../../core/md.js')
const seedData = require('../../../core/seed-data.js')

Page(withTheme({
  data: {
    stacks: [],
    activeStack: '',
    query: '',
    list: [],
    reading: null,
    html: ''
  },

  onLoad () {
    this.setData({ stacks: khc.STACK_META })
    this.refresh()
  },

  refresh () {
    const stack = this.data.activeStack
    const q = (this.data.query || '').trim().toLowerCase()

    let list = seedData.LEARN_DEEP
    if (stack) list = list.filter(function (a) { return a.stack === stack })
    if (q) {
      list = list.filter(function (a) {
        return a.title.toLowerCase().indexOf(q) >= 0 || a.stack.indexOf(q) >= 0
      })
    }

    this.setData({
      list: list.map(function (a) {
        const meta = khc.stackMeta(a.stack)
        return {
          slug: a.slug,
          title: a.title,
          stackLabel: meta.label,
          color: meta.color,
          summary: md.preview(a.md, 56)
        }
      })
    })
  },

  onPickStack (e) {
    const key = e.currentTarget.dataset.key
    this.setData({ activeStack: this.data.activeStack === key ? '' : key })
    this.refresh()
  },

  onSearch (e) {
    this.setData({ query: e.detail.value })
    this.refresh()
  },

  onOpen (e) {
    const slug = e.currentTarget.dataset.slug
    let found = null
    for (let i = 0; i < seedData.LEARN_DEEP.length; i++) {
      if (seedData.LEARN_DEEP[i].slug === slug) found = seedData.LEARN_DEEP[i]
    }
    if (!found) return
    this.setData({
      reading: found,
      html: md.render(found.md, this.data.isDark)
    })
    wx.setNavigationBarTitle({ title: found.title })
  },

  onClose () {
    this.setData({ reading: null, html: '' })
    wx.setNavigationBarTitle({ title: '图解系列' })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 图解系列', path: '/pages/learn/deep/index' }
  }
}))