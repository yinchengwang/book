/**
 * 知识图谱 —— Canvas 关系图 + 节点列表（点选/调级）
 *
 * 原版用 SVG 绘制，小程序不支持 SVG DOM，改为 <canvas type="2d">；
 * 坐标算法抽到 core/graph-layout.js（纯函数，可单测）。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const khc = require('../../../core/kh-constants.js')
const graphLayout = require('../../../core/graph-layout.js')

Page(withTheme({
  data: {
    nodes: [],
    links: [],
    selectedId: '',
    selected: null,
    related: [],
    activeDomain: '',
    domains: khc.KNOWLEDGE_DOMAINS
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const nodes = stores.knowledge.getAll()
    const links = stores.knowledge.links
    const domain = this.data.activeDomain
    const filtered = domain ? nodes.filter(function (n) { return n.domain === domain }) : nodes

    const rows = filtered.map(function (n) {
      return {
        id: n.id,
        name: n.name,
        domain: n.domain,
        level: n.level,
        color: graphLayout.domainColor(n.domain)
      }
    })

    this.setData({
      nodes: nodes,
      links: links,
      selectedId: this.data.selectedId,
      list: rows,
      selected: this.data.selectedId ? rows.filter(function (n) { return n.id === this.data.selectedId }, this)[0] || null : null
    })

    this.syncRelated()
  },

  /** 找出与选中节点有前置关系（双向）的节点 */
  syncRelated () {
    const id = this.data.selectedId
    if (!id) {
      this.setData({ related: [] })
      return
    }
    const related = []
    this.data.links.forEach(function (l) {
      if (l.source === id && l.target) related.push({ id: l.target, dir: '后续' })
      if (l.target === id && l.source) related.push({ id: l.source, dir: '前置' })
    })
    const nodes = this.data.nodes
    this.setData({
      related: related.map(function (r) {
        const n = nodes.filter(function (x) { return x.id === r.id })[0]
        return {
          id: r.id,
          name: n ? n.name : r.id,
          domain: n ? n.domain : '',
          dir: r.dir,
          color: n ? graphLayout.domainColor(n.domain) : '#64748B'
        }
      })
    })
  },

  onPickDomain (e) {
    const key = e.currentTarget.dataset.key
    this.setData({ activeDomain: this.data.activeDomain === key ? '' : key })
    this.refresh()
  },

  onSelect (e) {
    this.setData({ selectedId: e.currentTarget.dataset.id })
    this.refresh()
  },

  onAdjust (e) {
    const delta = Number(e.currentTarget.dataset.delta)
    const id = this.data.selectedId
    if (!id) return
    const n = stores.knowledge.find(id)
    if (!n) return
    stores.knowledge.updateNodeLevel(id, n.level + delta)
    this.refresh()
    wx.showToast({ title: '掌握度 ' + stores.knowledge.find(id).level, icon: 'none' })
  },

  onClose () {
    this.setData({ selectedId: '' })
    this.refresh()
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 知识图谱', path: '/pages/knowledge/graph/index' }
  }
}))