/**
 * 技术雷达 —— 知识点气泡图 / 技术栈雷达 / 能力差距（合并原 radar + gap_analysis）
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const khc = require('../../../core/kh-constants.js')
const seedData = require('../../../core/seed-data.js')
const radarLayout = require('../../../core/radar-layout.js')

Page(withTheme({
  data: {
    mode: 'quadrant',
    modes: [
      { key: 'quadrant', label: '知识点' },
      { key: 'stack', label: '技术栈' },
      { key: 'gap', label: '能力差距' }
    ],
    items: [],
    rows: [],
    gaps: [],
    quadrants: radarLayout.QUADRANT_ORDER,
    rings: radarLayout.RING_ORDER,
    activeQuadrant: '',
    activeRing: '',
    list: [],
    stats: null
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const items = seedData.RADAR_ITEMS.map(function (it, i) {
      return {
        id: it.id,
        title: it.title,
        quadrant: it.quadrant,
        ring: it.ring,
        tags: it.tags || [],
        recommend: (i % 4) + 1,
        desc: it.desc
      }
    })

    const rows = khc.STACK_META.map(function (s) {
      const prog = stores.quizzes.getProgress(s.key)
      const items2 = items.filter(function (it) {
        return (it.tags || []).indexOf(s.key) >= 0
      })
      // 掌握度 = 已答题比例；若该栈无标签知识点，则用题库进度兜底
      const percent = prog.total ? Math.round(prog.answered / prog.total * 100) : (items2.length ? 20 : 0)
      return {
        key: s.key,
        label: s.label,
        color: s.color,
        percent: percent,
        total: prog.total,
        answered: prog.answered
      }
    })

    const gapRaw = stores.knowledge.getGaps()
    const gaps = Object.keys(gapRaw).map(function (k) {
      const g = gapRaw[k]
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
        urgent: gap > 20,
        advice: gap > 20 ? '建议优先安排专项练习' : (gap > 0 ? '持续积累中' : '已达标')
      }
    }).sort(function (a, b) { return b.gap - a.gap })

    const q = this.data.activeQuadrant
    const r = this.data.activeRing
    let list = items
    if (q) list = list.filter(function (it) { return it.quadrant === q })
    if (r) list = list.filter(function (it) { return it.ring === r })

    this.setData({
      items: items,
      rows: rows,
      gaps: gaps,
      stats: { items: items.length, stacks: rows.length, avgPercent: Math.round(rows.reduce(function (a, b) { return a + b.percent }, 0) / (rows.length || 1)) },
      list: list.map(function (it) {
        const qm = radarLayout.QUADRANT_META[it.quadrant]
        const rm = radarLayout.RING_META[it.ring]
        return {
          id: it.id,
          title: it.title,
          desc: it.desc,
          quadrant: it.quadrant,
          quadrantLabel: qm.label,
          color: qm.color,
          ringLabel: rm.label
        }
      })
    })
  },

  onMode (e) {
    this.setData({ mode: e.currentTarget.dataset.mode })
  },

  onPickQuadrant (e) {
    const q = e.currentTarget.dataset.key
    this.setData({ activeQuadrant: this.data.activeQuadrant === q ? '' : q })
    this.refresh()
  },

  onPickRing (e) {
    const r = e.currentTarget.dataset.key
    this.setData({ activeRing: this.data.activeRing === r ? '' : r })
    this.refresh()
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 技术雷达', path: '/pages/knowledge/radar/index' }
  }
}))