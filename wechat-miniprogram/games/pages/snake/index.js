/**
 * 贪吃蛇 —— 原生小程序实现
 *
 * canvas 渲染 + 触摸滑动控制方向 + 定时器驱动 tick。
 */

const snake = require('../../utils/snake.js')
const storage = require('../../utils/storage.js')
const achievements = require('../../utils/achievements.js')
const share = require('../../utils/share.js')
const vibrate = require('../../utils/vibrate.js')

Page({
  data: {
    score: 0,
    best: 0,
    status: 'ready', // ready | playing | paused | over
    gridSize: snake.SNAKE_CONFIG.GRID_SIZE,
    gridWidth: snake.SNAKE_CONFIG.GRID_WIDTH,
    gridHeight: snake.SNAKE_CONFIG.GRID_HEIGHT,
    achievementToast: null
  },

  onLoad () {
    this.state = snake.createInitialState()
    this.timer = null
    this.touchStart = { x: 0, y: 0 }
    this.best = storage.getSnakeBestScore()

    this.unsubscribe = achievements.subscribe(a => {
      this.setData({ achievementToast: a })
    })

    this.setData({ best: this.best })
  },

  onUnload () {
    this.stopLoop()
    if (this.unsubscribe) this.unsubscribe()
  },

  /** 开始 / 重新开始 */
  onStart () {
    this.state = snake.createInitialState()
    this.setData({ score: 0, status: 'playing' })
    this.startLoop()
    this.draw()
  },

  /** 暂停 */
  onPause () {
    if (this.data.status !== 'playing') return
    this.stopLoop()
    this.setData({ status: 'paused' })
  },

  /** 继续 */
  onResume () {
    if (this.data.status !== 'paused') return
    this.setData({ status: 'playing' })
    this.startLoop()
  },

  /** 启动定时循环 */
  startLoop () {
    this.stopLoop()
    const self = this
    this.timer = setInterval(function () {
      self.step()
    }, this.state.speed)
  },

  /** 停止定时循环 */
  stopLoop () {
    if (this.timer) {
      clearInterval(this.timer)
      this.timer = null
    }
  },

  /** 单步推进 */
  step () {
    const self = this
    snake.tick(this.state)

    if (this.state.score > 0 && this.state.score % 10 === 0) {
      // 每吃一个食物触发一次成就评估
      achievements.evaluate({ type: 'snake.eat', score: this.state.score })
      vibrate.trigger()
    }

    if (this.state.isGameOver) {
      this.stopLoop()
      this.onGameOver()
      return
    }

    // 速度随得分提升，同步重启定时器
    const prevSpeed = this.timer
    if (prevSpeed) clearInterval(prevSpeed)
    this.timer = setInterval(function () { self.step() }, this.state.speed)

    this.setData({ score: this.state.score })
    this.draw()
  },

  /** 游戏结束结算 */
  onGameOver () {
    const score = this.state.score
    storage.updateSnakeBestScore(score)
    achievements.evaluate({ type: 'snake.gameOver', score })

    this.setData({
      status: 'over',
      score,
      best: storage.getSnakeBestScore()
    })
    this.draw()

    wx.vibrateLong()
  },

  /** 触摸开始：记录起点 */
  onTouchStart (e) {
    const touch = e.touches[0]
    this.touchStart = { x: touch.clientX, y: touch.clientY }
  },

  /** 触摸结束：识别滑动方向 */
  onTouchEnd (e) {
    if (this.data.status !== 'playing') return
    const touch = e.changedTouches[0]
    const dx = touch.clientX - this.touchStart.x
    const dy = touch.clientY - this.touchStart.y

    // 阈值过滤，避免误触
    if (Math.abs(dx) < 20 && Math.abs(dy) < 20) return

    const D = snake.Direction
    if (Math.abs(dx) > Math.abs(dy)) {
      snake.setDirection(this.state, dx > 0 ? D.RIGHT : D.LEFT)
    } else {
      snake.setDirection(this.state, dy > 0 ? D.DOWN : D.UP)
    }
  },

  /** 方向按钮（备用操作方式） */
  onDirection (e) {
    if (this.data.status !== 'playing') return
    const map = {
      up: snake.Direction.UP,
      down: snake.Direction.DOWN,
      left: snake.Direction.LEFT,
      right: snake.Direction.RIGHT
    }
    const dir = map[e.currentTarget.dataset.dir]
    if (dir !== undefined) snake.setDirection(this.state, dir)
  },

  /** canvas 绘制 */
  draw () {
    const self = this
    const query = wx.createSelectorQuery()
    query.select('#snake-canvas')
      .fields({ node: true, size: true })
      .exec(function (res) {
        if (!res || !res[0] || !res[0].node) return
        const canvas = res[0].node
        const ctx = canvas.getContext('2d')

        const dpr = wx.getWindowInfo ? wx.getWindowInfo().pixelRatio : wx.getSystemInfoSync().pixelRatio
        canvas.width = res[0].width * dpr
        canvas.height = res[0].height * dpr
        ctx.scale(dpr, dpr)

        const cols = snake.SNAKE_CONFIG.GRID_WIDTH
        const rows = snake.SNAKE_CONFIG.GRID_HEIGHT
        const cell = Math.min(res[0].width / cols, res[0].height / rows)
        const offsetX = (res[0].width - cell * cols) / 2
        const offsetY = (res[0].height - cell * rows) / 2

        const grid = snake.getGrid(self.state)

        ctx.clearRect(0, 0, res[0].width, res[0].height)

        for (let r = 0; r < rows; r++) {
          for (let c = 0; c < cols; c++) {
            const x = offsetX + c * cell
            const y = offsetY + r * cell
            const v = grid[r][c]

            if (v === 0) continue

            ctx.beginPath()
            ctx.fillStyle = v === 2 ? '#ff6b6b' : v === 3 ? '#51cf66' : '#38d9a9'
            const inset = v === 3 ? 1 : 2
            self.roundRect(ctx, x + inset, y + inset, cell - inset * 2, cell - inset * 2, v === 3 ? 6 : 4)
            ctx.fill()
          }
        }
      })
  },

  /** 圆角矩形路径 */
  roundRect (ctx, x, y, w, h, r) {
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
    const card = share.buildSnakeShare(this.data.score, this.data.best)
    return { title: card.title, path: card.path }
  }
})