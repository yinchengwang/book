/**
 * 消消乐 —— 原生小程序实现
 *
 * 章节选择 → 9x9 棋盘拖拽交换 → 连锁消除 → 目标达成。
 */

const match3 = require('../../utils/match3.js')
const storage = require('../../utils/storage.js')
const achievements = require('../../utils/achievements.js')
const share = require('../../utils/share.js')
const vibrate = require('../../utils/vibrate.js')

Page({
  data: {
    phase: 'menu', // menu | play
    chapters: [],
    totalStars: 0,

    // 游戏中
    level: 1,
    chapter: 1,
    cells: [],
    moves: 0,
    maxMoves: 0,
    score: 0,
    goals: [],
    status: 'playing', // playing | won | lost
    selected: '',
    stars: 0,
    chapterName: '',
    levelTypeName: '',
    levelTypeColor: '',
    achievementToast: null
  },

  onLoad () {
    this.state = null
    this.busy = false
    this.unsubscribe = achievements.subscribe(a => {
      this.setData({ achievementToast: a })
    })
    this.loadChapters()
  },

  onUnload () {
    if (this.unsubscribe) this.unsubscribe()
  },

  /** 加载章节 + 解锁 + 星数 */
  loadChapters () {
    const unlocked = storage.getMatch3UnlockedChapters()
    const earned = storage.getMatch3EarnedStars()
    const totalStars = storage.getMatch3TotalStars()

    const chapters = match3.CHAPTERS.map(c => {
      let stars = 0
      let cleared = 0
      for (let lv = c.levels[0]; lv <= c.levels[1]; lv++) {
        const s = earned[lv] || 0
        stars += s
        if (s > 0) cleared++
      }
      return {
        id: c.id,
        name: c.name,
        type: c.type,
        typeName: match3.LEVEL_TYPE_INFO[c.type].name,
        typeColor: match3.LEVEL_TYPE_INFO[c.type].color,
        description: c.description,
        levelsText: c.levels[0] + '-' + c.levels[1],
        unlocked: match3.isChapterUnlocked(c, totalStars, unlocked),
        stars,
        maxStars: (c.levels[1] - c.levels[0] + 1) * 3,
        cleared,
        totalLevels: c.levels[1] - c.levels[0] + 1
      }
    })

    this.setData({ chapters, totalStars })
  },

  /** 进入章节 */
  onChapterTap (e) {
    const id = Number(e.currentTarget.dataset.id)
    const chapter = this.data.chapters.filter(c => c.id === id)[0]
    if (!chapter) return
    if (!chapter.unlocked) {
      wx.showToast({ title: '星数不足，章节未解锁', icon: 'none' })
      return
    }
    this.setData({ currentChapter: id })
  },

  /** 开始关卡 */
  onLevelTap (e) {
    const level = Number(e.currentTarget.dataset.level)
    const config = match3.getLevelConfig(level)

    this.state = match3.createGameState(
      level,
      storage.getMatch3EarnedStars(),
      storage.getMatch3UnlockedChapters()
    )

    const chapter = match3.getChapterByLevel(level)
    const typeInfo = match3.LEVEL_TYPE_INFO[config.type]

    this.setData({
      phase: 'play',
      level,
      chapter: config.chapter,
      chapterName: chapter ? chapter.name : '',
      levelTypeName: typeInfo.name,
      levelTypeColor: typeInfo.color,
      cells: this.buildCells(),
      moves: this.state.moves,
      maxMoves: this.state.maxMoves,
      score: this.state.score,
      goals: this.buildGoals(),
      status: 'playing',
      selected: ''
    })
  },

  /**
   * 把棋盘拍平成 81 格，供 wx:for 渲染。
   * 每格附带宝石配色、障碍物与特殊元素信息。
   */
  buildCells () {
    const cells = []
    const size = match3.BOARD_SIZE
    for (let r = 0; r < size; r++) {
      for (let c = 0; c < size; c++) {
        const cell = this.state.board[r][c]
        const gem = cell.gem
        const gemColors = gem === null ? null : match3.GEM_COLORS[gem]
        const obstacle = cell.obstacle ? match3.getObstacleRenderInfo(cell) : null
        const element = cell.element ? match3.ELEMENT_INFO[cell.element] : null

        cells.push({
          row: r,
          col: c,
          pos: r + ',' + c,
          hasGem: gem !== null,
          bg: gemColors ? gemColors.primary : 'transparent',
          shadow: gemColors ? gemColors.secondary : 'transparent',
          obstacleEmoji: obstacle ? obstacle.emoji : '',
          obstacleHp: obstacle ? obstacle.hp : 0,
          obstacleMax: obstacle ? obstacle.maxHp : 0,
          obstacleColor: obstacle ? obstacle.color : '',
          obstacleBorder: obstacle ? obstacle.borderColor : '',
          obstacleDestroyed: obstacle ? obstacle.destroyed : false,
          elementEmoji: element ? element.emoji : ''
        })
      }
    }
    return cells
  },

  /** 目标进度（含 emoji） */
  buildGoals () {
    return this.state.goals.map(g => ({
      type: g.type,
      current: Math.min(g.current, g.target),
      target: g.target,
      emoji: this.goalEmoji(g.type),
      done: g.current >= g.target
    }))
  },

  goalEmoji (type) {
    if (type === 'score') return '⭐'
    const info = match3.ELEMENT_INFO[type]
    return info ? info.emoji : '❓'
  },

  /** 点击格子：两段式选择 → 交换 */
  onCellTap (e) {
    if (this.busy || this.data.status !== 'playing') return
    const pos = e.currentTarget.dataset.pos

    if (!this.data.selected) {
      this.setData({ selected: pos })
      return
    }

    if (this.data.selected === pos) {
      this.setData({ selected: '' })
      return
    }

    if (!match3.isValidSwap(this.state.board, this.data.selected, pos)) {
      wx.showToast({ title: '只能交换相邻宝石', icon: 'none' })
      this.setData({ selected: '' })
      return
    }

    const from = this.data.selected
    this.setData({ selected: '' })
    this.attemptSwap(from, pos)
  },

  /** 尝试交换，失败则回弹 */
  attemptSwap (pos1, pos2) {
    const self = this
    this.busy = true

    match3.doSwap(this.state.board, pos1, pos2)
    const matches = match3.findMatches(this.state.board)

    if (matches.length === 0) {
      // 无匹配 → 300ms 后回弹
      setTimeout(function () {
        match3.doSwap(self.state.board, pos1, pos2)
        self.busy = false
        self.setData({ cells: self.buildCells() })
      }, 280)
      return
    }

    this.resolveMatches(matches)
  },

  /**
   * 连锁结算主循环
   * @param {Array} matches 首轮匹配
   */
  resolveMatches (matches) {
    const self = this
    let combo = 0
    let totalRemoved = 0

    const step = function (currentMatches) {
      combo++

      const positions = new Set()
      const shape = match3.analyzeMatchShape(currentMatches)

      for (let i = 0; i < currentMatches.length; i++) {
        currentMatches[i].cells.forEach(p => positions.add(p))
      }

      // 连锁计数进目标进度
      if (combo > 1) {
        wx.vibrateShort({ type: 'light' })
      }

      // 4 连 → 直线特效；5 连 → 彩虹；交叉 → 炸弹
      let special = null
      let specialPos = null
      if (shape.line5.length > 0) {
        special = match3.SpecialType.RAINBOW
        specialPos = shape.line5[0]
      } else if (shape.intersections.length > 0) {
        special = match3.SpecialType.BOMB
        specialPos = shape.intersections[0]
      } else if (shape.line4.length > 0) {
        special = match3.SpecialType.ROW_CLEAR
        specialPos = shape.line4[0]
      }

      if (special && specialPos) {
        match3.triggerSpecial(self.state.board, specialPos, special).forEach(p => positions.add(p))
      }

      self.applyRemoval(positions, combo)

      totalRemoved += positions.size
      self.state.score += match3.calculateScore(positions.size, combo)

      // 下落 + 补充
      match3.dropGems(self.state.board)

      const nextMatches = match3.findMatches(self.state.board)
      if (nextMatches.length === 0) {
        // 连锁结束，扣步数
        self.state.moves--
        self.afterTurn()
        return
      }

      // 连锁间隔，给出视觉反馈
      setTimeout(function () { step(nextMatches) }, 260)
    }

    step(matches)
  },

  /**
   * 执行消除：伤害障碍物、收集特殊元素、判定胜负
   * @param {Set<string>} positions 被消除的位置集合
   */
  applyRemoval (positions, combo) {
    const self = this
    positions.forEach(pos => {
      const p = pos.split(',').map(Number)
      const cell = self.state.board[p[0]][p[1]]

      // 特殊元素收集
      if (cell.element) {
        self.state.collected[cell.element] = (self.state.collected[cell.element] || 0) + 1
        cell.element = null
      }

      // 障碍物伤害
      if (cell.obstacle) {
        match3.damageObstacle(cell, 1)
        if (match3.isObstacleDestroyed(cell)) {
          match3.clearObstacle(cell)
        }
        return // 障碍未破则宝石不消
      }

      // 普通宝石：若为特效宝石则触发
      if (cell.special) {
        match3.triggerSpecial(self.state.board, pos, cell.special).forEach(p2 => {
          const q = p2.split(',').map(Number)
          if (self.state.board[q[0]][q[1]].gem !== null) {
            self.state.board[q[0]][q[1]].gem = null
          }
        })
      }

      cell.gem = null
      cell.special = null
    })

    // 同步目标进度
    this.state.goals.forEach(g => {
      if (g.type === 'score') {
        g.current = this.state.score
      } else {
        g.current = this.state.collected[g.type] || 0
      }
    })
  },

  /** 一回合结束后的结算 */
  afterTurn () {
    // 蜗牛推进
    const snailResult = match3.updateSnails(this.state)

    // 蜗牛吃掉宝石
    snailResult.snailRemoved.forEach(pos => {
      const p = pos.split(',').map(Number)
      if (p[0] >= 0 && p[0] < match3.BOARD_SIZE) {
        this.state.board[p[0]][p[1]].gem = null
      }
    })

    const goalsDone = match3.checkGoalsComplete(this.state)
    const movesOut = this.state.moves <= 0

    this.setData({
      cells: this.buildCells(),
      moves: Math.max(0, this.state.moves),
      score: this.state.score,
      goals: this.buildGoals()
    })
    vibrate.trigger()

    if (snailResult.gameOver) {
      this.finish('lost')
      return
    }
    if (goalsDone) {
      this.finish('won')
      return
    }
    if (movesOut) {
      this.finish('lost')
      return
    }

    this.busy = false
  },

  /** 结束本关 */
  finish (result) {
    this.busy = false

    if (result === 'won') {
      const stars = match3.calculateStars(
        this.state.score,
        match3.getLevelConfig(this.state.level).targetScore,
        Math.max(0, this.state.moves),
        this.state.maxMoves
      )
      this.state.status = 'won'
      this.state.stars = stars

      storage.setMatch3Stars(this.state.level, stars)
      achievements.evaluate({ type: 'match3.complete', stars })

      // 每章最后一关解锁下一章
      const chapter = match3.getChapterByLevel(this.state.level)
      if (chapter && this.state.level === chapter.levels[1] && chapter.id < match3.CHAPTERS.length) {
        storage.unlockMatch3Chapter(chapter.id + 1)
      }
      this.loadChapters()
    } else {
      this.state.status = 'lost'
    }

    this.setData({
      status: result === 'won' ? 'won' : 'lost',
      stars: this.state.stars
    })
  },

  /** 重玩本关 */
  onRetry () {
    this.onLevelTap({ currentTarget: { dataset: { level: this.data.level } } })
  },

  /** 返回章节 */
  onBack () {
    this.setData({ phase: 'menu' })
    this.loadChapters()
  },

  onShareAppMessage () {
    const card = share.buildMatch3Share(this.data.chapter, this.data.level, this.data.score)
    return { title: card.title, path: card.path }
  }
})