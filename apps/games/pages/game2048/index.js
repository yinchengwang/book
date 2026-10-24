/**
 * 2048 —— 原生小程序实现
 *
 * canvas 渲染 + 滑动/按钮控制四方向移动。
 */

const g2048 = require('../../utils/game2048.js')
const storage = require('../../utils/storage.js')
const achievements = require('../../utils/achievements.js')
const share = require('../../utils/share.js')
const vibrate = require('../../utils/vibrate.js')

Page({
  data: {
    score: 0,
    best: 0,
    status: 'ready', // ready | playing | over
    hasWon: false,
    keepGoing: false,
    gridSize: g2048.CONFIG.GRID_SIZE,
    achievementToast: null
  },

  onLoad () {
    this.best = storage.get2048BestScore()
    this.state = g2048.createInitialState(this.best)
    this.touchStart = { x: 0, y: 0 }

    this.unsubscribe = achievements.subscribe(a => {
      this.setData({ achievementToast: a })
    })

    this.setData({ best: this.best })
  },

  onReady () {
    this.draw()
  },

  onUnload () {
    if (this.unsubscribe) this.unsubscribe()
  },

  onStart () {
    this.state = g2048.createInitialState(storage.get2048BestScore())
    this.setData({
      score: 0,
      status: 'playing',
      hasWon: false,
      keepGoing: false
    })
    this.draw()
  },

  /** 达成 2048 后选择继续 */
  onKeepGoing () {
    g2048.keepGoing(this.state)
    this.setData({ keepGoing: true })
  },

  /** 达成 2048 后选择结束本局 */
  onEndWin () {
    this.finishGame()
  },

  /** 执行移动 */
  doMove (direction) {
    if (this.data.status !== 'playing') return
    if (this.state.hasWon && !this.state.keepGoing) return

    const result = g2048.move(this.state, direction)
    if (!result.moved) return

    // 合并出 2048 / 4096 时评估成就
    if (result.mergedTile >= 2048) {
      achievements.evaluate({ type: 'game2048.merge', tileValue: result.mergedTile })
    }

    const won = this.state.hasWon
    const gameOver = this.state.isGameOver

    this.setData({
      score: this.state.score,
      best: Math.max(this.data.best, this.state.bestScore),
      hasWon: won,
      keepGoing: this.state.keepGoing
    })

    vibrate.trigger()
    this.draw()

    if (gameOver) {
      this.finishGame()
    }
  },

  /** 结束并持久化最高分 */
  finishGame () {
    storage.update2048BestScore(this.state.score)
    this.setData({
      status: 'over',
      best: storage.get2048BestScore()
    })
    this.draw()
  },

  onTouchStart (e) {
    const touch = e.touches[0]
    this.touchStart = { x: touch.clientX, y: touch.clientY }
  },

  onTouchEnd (e) {
    if (this.data.status !== 'playing') return
    const touch = e.changedTouches[0]
    const dx = touch.clientX - this.touchStart.x
    const dy = touch.clientY - this.touchStart.y
    if (Math.abs(dx) < 20 && Math.abs(dy) < 20) return

    if (Math.abs(dx) > Math.abs(dy)) {
      this.doMove(dx > 0 ? 'right' : 'left')
    } else {
      this.doMove(dy > 0 ? 'down' : 'up')
    }
  },

  /** 方向按钮 */
  onDirection (e) {
    this.doMove(e.currentTarget.dataset.dir)
  },

  draw () {
    const self = this
    const query = wx.createSelectorQuery()
    query.select('#g2048-canvas')
      .fields({ node: true, size: true })
      .exec(function (res) {
        if (!res || !res[0] || !res[0].node) return
        const canvas = res[0].node
        const ctx = canvas.getContext('2d')

        const dpr = wx.getWindowInfo ? wx.getWindowInfo().pixelRatio : wx.getSystemInfoSync().pixelRatio
        canvas.width = res[0].width * dpr
        canvas.height = res[0].height * dpr
        ctx.scale(dpr, dpr)

        const size = g2048.CONFIG.GRID_SIZE
        const cell = res[0].width / size
        const gap = 8
        const pad = gap

        ctx.clearRect(0, 0, res[0].width, res[0].height)

        // 棋盘底色
        ctx.fillStyle = '#3c3a32'
        self.roundRect(ctx, 0, 0, res[0].width, res[0].width, 12)
        ctx.fill()

        const board = self.state.board
        for (let r = 0; r < size; r++) {
          for (let c = 0; c < size; c++) {
            const value = board[r][c]
            if (value === 0) continue

            const x = pad + c * cell + gap / 2
            const y = pad + r * cell + gap / 2
            const w = cell - gap
            const h = cell - gap

            ctx.fillStyle = g2048.getTileColor(value)
            self.roundRect(ctx, x, y, w, h, 8)
            ctx.fill()

            ctx.fillStyle = g2048.getTextColor(value)
            ctx.font = 'bold ' + (value >= 1024 ? 28 : value >= 128 ? 34 : 40) + 'px sans-serif'
            ctx.textAlign = 'center'
            ctx.textBaseline = 'middle'
            ctx.fillText(String(value), x + w / 2, y + h / 2)
          }
        }
      })
  },

  roundRect (ctx, x, y, w, h, r) {
    ctx.beginPath()
    ctx.moveTo(x + r, y)
    ctx.lineTo(x + w - r, y)
    ctx.quadraticCurveTo(x + w, y, x + w, y + r)
    ctx.lineTo(x + w, y + h - r)
    ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
    ctx.lineTo(x + r, y + h)
    ctx.quadraticCurveTo(x, y + h, x, y + h - r)
    ctx.lineTo(x, y + r)
    ctx.quadraticCurveTo(x, y, x + r, y)
    ctx.closePath()
  },

  onShareAppMessage () {
    const card = share.build2048Share(this.data.score, this.data.best)
    return { title: card.title, path: card.path }
  }
})