/**
 * 知识 —— 4 个模块聚合入口（含能力雷达图与图谱概览）
 */

const { withTheme } = require('../../core/with-theme.js')
const stores = require('../../core/stores.js')
const khc = require('../../core/kh-constants.js')

Page(withTheme({
  data: {
    entries: [],
    stats: null,
    gaps: []
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const nodes = stores.knowledge.getAll()
    const excerpts = stores.excerpts.derive()
    const radar = require('../../core/seed-data.js').RADAR_ITEMS

    const gaps = stores.knowledge.getGaps()
    const gapRows = Object.keys(gaps).map(function (k) {
      const g = gaps[k]
      const meta = khc.KNOWLEDGE_DOMAINS.filter(function (d) { return d.key === k })[0]
      const gap = Math.max(0, g.target - g.level)
      return {
        key: k,
        label: meta ? meta.label : k,
        color: meta ? meta.color : '#64748B',
        level: g.level,
        target: g.target,
        gap: gap,
        percent: g.target ? Math.min(100, Math.round(g.level / g.target * 100)) : 0,
        urgent: gap > 20
      }
    }).sort(function (a, b) { return b.gap - a.gap })

    this.setData({
      stats: { nodes: nodes.length, links: stores.knowledge.links.length, excerpts: excerpts.stats.total, radar: radar.length },
      gaps: gapRows,
      entries: [
        {
          key: 'graph', symbol: 'G', title: '知识图谱', color: '#6366F1',
          desc: '节点关系与前置依赖', path: '/pages/knowledge/graph/index', count: nodes.length
        },
        {
          key: 'excerpt', symbol: 'E', title: '摘抄管理', color: '#30A46C',
          desc: '书籍摘录 · 收藏与状态', path: '/pages/knowledge/excerpt/index', count: excerpts.stats.total
        },
        {
          key: 'radar', symbol: '◎', title: '技术雷达', color: '#F5A524',
          desc: '知识点分布与能力差距', path: '/pages/knowledge/radar/index', count: radar.length
        }
      ]
    })
  },

  onOpen (e) {
    const path = e.currentTarget.dataset.path
    if (path) wx.navigateTo({ url: path })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 知识', path: '/pages/knowledge/index' }
  }
}))