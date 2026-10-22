/**
 * radar-chart —— 技术雷达画布（Canvas 2d）
 *
 * 原 H5 版用 SVG（气泡图 / 雷达多边形 + 力导向碰撞），小程序改为 Canvas。
 * 坐标计算全部在 core/radar-layout.js，本组件只负责取 node 与绘制。
 *
 * 属性：mode（'quadrant' | 'stack'）/ items / rows / isDark
 */

const layout = require('../../core/radar-layout.js')

Component({
  properties: {
    mode: { type: String, value: 'quadrant' },
    items: { type: Array, value: [] },
    rows: { type: Array, value: [] },
    isDark: { type: Boolean, value: false }
  },

  data: {
    ready: false,
    width: 0,
    height: 0
  },

  observers: {
    'mode,items,rows,isDark': function () {
      if (this.data.ready) this.draw()
    }
  },

  lifetimes: {
    ready () {
      const self = this
      wx.createSelectorQuery().in(this).select('#radar').fields({ node: true, size: true }).exec(function (res) {
        const info = res && res[0]
        if (!info || !info.node) return
        const canvas = info.node
        const ctx = canvas.getContext('2d')
        const dpr = (wx.getSystemInfoSync().pixelRatio) || 2
        canvas.width = info.width * dpr
        canvas.height = info.height * dpr
        ctx.scale(dpr, dpr)
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
      const w = this.data.width
      const h = this.data.height
      ctx.clearRect(0, 0, w, h)

      if (this.data.mode === 'stack') this.drawStack(ctx, w, h)
      else this.drawQuadrant(ctx, w, h)
    },

    /** 象限气泡图 */
    drawQuadrant (ctx, w, h) {
      const items = this.data.items
      if (!items.length) return
      const geo = layout.quadrantBubbles(items, { width: w, height: h })

      // 象限轴线
      ctx.strokeStyle = this.data.isDark ? '#33404F' : '#E4E7EC'
      ctx.lineWidth = 1
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2
        ctx.beginPath()
        ctx.moveTo(geo.cx, geo.cy)
        ctx.lineTo(geo.cx + Math.cos(a) * geo.radius, geo.cy + Math.sin(a) * geo.radius)
        ctx.stroke()
      }

      // 碰撞推开
      const adjusted = layout.resolveCollisions(geo.points, { minGap: 24, rounds: 8 })

      // 气泡
      adjusted.forEach(function (p, i) {
        const src = geo.points[i]
        ctx.beginPath()
        ctx.arc(p.x, p.y, src.r, 0, Math.PI * 2)
        ctx.fillStyle = src.color
        ctx.globalAlpha = 0.7
        ctx.fill()
        ctx.globalAlpha = 1
      }, this)
    },

    /** 栈雷达多边形 */
    drawStack (ctx, w, h) {
      const rows = this.data.rows
      if (!rows.length) return
      const geo = layout.stackPolygon(rows, { width: w, height: h })

      // 网格环
      ctx.strokeStyle = this.data.isDark ? '#232B38' : '#E4E7EC'
      ctx.lineWidth = 1
      ;[0.25, 0.5, 0.75, 1].forEach(function (ratio) {
        ctx.beginPath()
        geo.axes.forEach(function (ax, i) {
          const angle = (i / geo.axes.length) * Math.PI * 2 - Math.PI / 2
          const rr = geo.radius * ratio
          const x = geo.cx + Math.cos(angle) * rr
          const y = geo.cy + Math.sin(angle) * rr
          if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        })
        ctx.closePath()
        ctx.stroke()
      })

      // 轴线
      ctx.beginPath()
      geo.axes.forEach(function (ax, i) {
        if (i === 0) ctx.moveTo(geo.cx, geo.cy); else ctx.lineTo(ax.x, ax.y)
      })
      ctx.stroke()

      // 数据多边形
      ctx.beginPath()
      geo.points.forEach(function (p, i) {
        if (i === 0) ctx.moveTo(p.x, p.y); else ctx.lineTo(p.x, p.y)
      })
      ctx.closePath()
      ctx.fillStyle = 'rgba(99, 102, 241, 0.24)'
      ctx.fill()
      ctx.strokeStyle = '#6366F1'
      ctx.lineWidth = 2
      ctx.stroke()
    }
  }
})