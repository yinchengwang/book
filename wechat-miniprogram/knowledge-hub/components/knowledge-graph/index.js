/**
 * knowledge-graph —— 知识图谱画布（Canvas 2d）
 *
 * 小程序 Canvas 注意事项（本组件已处理）：
 *   1. 必须在 ready 之后用 wx.createSelectorQuery() 异步取 node
 *   2. 画布尺寸用 px 而非 rpx，需乘 pixelRatio 保证高清屏清晰
 *   3. 节点 / 连线坐标由 core/graph-layout.js 纯计算得出，本组件只负责绘制
 *
 * 属性：nodes / links / selectedId / brand
 *
 * 节点点选由父页面以列表形式提供（Canvas 命中检测需 getTouches 换算坐标，
 * 小程序收益低而复杂度高，故节点交互走下方列表）。
 */

const layout = require('../../core/graph-layout.js')

Component({
  properties: {
    nodes: { type: Array, value: [] },
    links: { type: Array, value: [] },
    selectedId: { type: String, value: '' },
    brand: { type: String, value: '#6366F1' }
  },

  data: {
    ready: false,
    width: 0,
    height: 0
  },

  observers: {
    'nodes,links,selectedId': function () {
      if (this.data.ready) this.draw()
    }
  },

  lifetimes: {
    ready () {
      const self = this
      wx.createSelectorQuery().in(this).select('#graph').fields({ node: true, size: true }).exec(function (res) {
        const info = res && res[0]
        if (!info || !info.node) {
          self.setData({ ready: false })
          return
        }
        const canvas = info.node
        const ctx = canvas.getContext('2d')
        const dpr = (wx.getSystemInfoSync().pixelRatio) || 2
        // 用 px 画布 + dpr 放大，保证高清
        canvas.width = info.width * dpr
        canvas.height = info.height * dpr
        ctx.scale(dpr, dpr)
        self.__canvas = canvas
        self.__ctx = ctx
        self.setData({ ready: true, width: info.width, height: info.height }, function () {
          self.draw()
        })
      })
    }
  },

  methods: {
    draw () {
      const ctx = this.__ctx
      if (!ctx) return
      const nodes = this.data.nodes
      const links = this.data.links
      if (!nodes.length) return

      const w = this.data.width
      const h = this.data.height
      const geo = layout.layout(nodes, links, { width: w, height: h })

      ctx.clearRect(0, 0, w, h)

      // 连线（前置依赖）
      const pos = {}
      geo.points.forEach(function (p) { pos[p.id] = p })
      ctx.strokeStyle = this.data.brand
      ctx.globalAlpha = 0.35
      ctx.lineWidth = 1
      geo.edges.forEach(function (e) {
        const a = pos[e.from]
        const b = pos[e.to]
        ctx.beginPath()
        ctx.moveTo(a.x, a.y)
        ctx.lineTo(b.x, b.y)
        ctx.stroke()
      })

      // 节点
      ctx.globalAlpha = 1
      geo.points.forEach(function (p) {
        const selected = p.id === this.data.selectedId
        ctx.beginPath()
        ctx.arc(p.x, p.y, p.r, 0, Math.PI * 2)
        ctx.fillStyle = p.color
        ctx.globalAlpha = selected ? 1 : 0.75
        ctx.fill()
        ctx.globalAlpha = 1
        if (selected) {
          ctx.lineWidth = 2
          ctx.strokeStyle = '#FFFFFF'
          ctx.stroke()
        }
      }, this)
    }
  }
})