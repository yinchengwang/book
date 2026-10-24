/**
 * 数独 —— 原生小程序实现
 *
 * 章节选择 → 9x9 棋盘 → 数字键盘填入。
 */

const sudoku = require('../../utils/sudoku.js')
const storage = require('../../utils/storage.js')
const achievements = require('../../utils/achievements.js')
const share = require('../../utils/share.js')
const vibrate = require('../../utils/vibrate.js')

Page({
  data: {
    // 阶段：menu（选章节关卡）/ play（游戏中）
    phase: 'menu',
    chapters: [],
    status: 'ready', // ready | playing | won
    cells: [],
    cursorRow: 0,
    cursorCol: 0,
    selectedNumber: 1,
    emptyCount: 0,
    hintCount: 3,
    difficultyName: '',
    currentChapter: 0,
    currentLevel: 0,
    achievementToast: null
  },

  onLoad () {
    this.state = null
    this.unsubscribe = achievements.subscribe(a => {
      this.setData({ achievementToast: a })
    })
    this.loadChapters()
  },

  onUnload () {
    if (this.unsubscribe) this.unsubscribe()
  },

  /** 加载章节与星数进度 */
  loadChapters () {
    const saved = storage.getSudokuData()
    const chapters = saved.chapters.map(ch => {
      const starKeys = Object.keys(ch.stars)
      const total = starKeys.reduce((sum, k) => sum + (ch.stars[k] || 0), 0)
      const maxStars = starKeys.length * 3
      return {
        id: ch.id,
        unlocked: ch.unlocked,
        totalStars: total,
        maxStars,
        filledStars: starKeys.length,
        levels: this.buildLevels(ch)
      }
    })
    this.setData({ chapters })
  },

  /** 生成该章节的 10 个关卡格子 */
  buildLevels (chapter) {
    const levels = []
    for (let i = 0; i < 10; i++) {
      const levelId = i + 1
      levels.push({
        id: levelId,
        stars: chapter.stars[levelId] || 0,
        // 未解锁的关卡：前一关拿到过星才开放
        unlocked: i === 0 || (chapter.stars[i] || 0) > 0
      })
    }
    return levels
  },

  /** 点击章节：解锁检查 */
  onChapterTap (e) {
    const id = Number(e.currentTarget.dataset.id)
    const chapter = this.data.chapters.filter(c => c.id === id)[0]
    if (!chapter) return

    if (!chapter.unlocked) {
      wx.showToast({ title: '请先完成上一章节', icon: 'none' })
      return
    }
    this.setData({ currentChapter: id })
  },

  /** 开始某一关 */
  onLevelTap (e) {
    const level = Number(e.currentTarget.dataset.level)
    if (level === 0) {
      wx.showToast({ title: '请先选择章节', icon: 'none' })
      return
    }
    if (e.currentTarget.dataset.unlocked !== 'true' && e.currentTarget.dataset.unlocked !== true) {
      wx.showToast({ title: '请先完成前一关', icon: 'none' })
      return
    }

    this.state = sudoku.createInitialState(0)
    // 难度随关卡递进：1-3 简单 / 4-6 中等 / 7-10 困难
    const difficulty = level <= 3 ? 0 : level <= 6 ? 1 : 2
    this.state = sudoku.createInitialState(difficulty)

    this.setData({
      phase: 'play',
      status: 'playing',
      currentLevel: level,
      difficultyName: sudoku.SUDOKU_CONFIG.DIFFICULTIES[difficulty].name,
      cells: this.buildCells(),
      cursorRow: this.state.cursorRow,
      cursorCol: this.state.cursorCol,
      selectedNumber: this.state.selectedNumber,
      emptyCount: this.state.emptyCount,
      hintCount: this.state.hintCount
    })
  },

  /**
   * 把 9x9 棋盘拍平成 81 个格子，供 wx:for 直接渲染。
   * 每格附带宫格分隔线标记（hasRight / hasBottom）。
   */
  buildCells () {
    const cells = []
    for (let r = 0; r < 9; r++) {
      for (let c = 0; c < 9; c++) {
        const cell = this.state.board[r][c]
        cells.push({
          row: r,
          col: c,
          value: cell.value,
          isGiven: cell.isGiven,
          isConflict: cell.isConflict,
          hasRight: c === 2 || c === 5,
          hasBottom: r === 2 || r === 5
        })
      }
    }
    return cells
  },

  /** 选择格子 */
  onCellTap (e) {
    if (this.data.status !== 'playing') return
    const row = Number(e.currentTarget.dataset.row)
    const col = Number(e.currentTarget.dataset.col)

    this.state.cursorRow = row
    this.state.cursorCol = col
    this.setData({ cursorRow: row, cursorCol: col })
  },

  /** 选择数字 */
  onNumberTap (e) {
    if (this.data.status !== 'playing') return
    const num = Number(e.currentTarget.dataset.num)
    sudoku.selectNumber(this.state, num)

    // 选中即填入（题目固定格不可改）
    sudoku.placeNumber(this.state, num)
    vibrate.trigger()
    this.syncState()
  },

  /** 擦除 */
  onErase () {
    if (this.data.status !== 'playing') return
    sudoku.eraseCell(this.state)
    this.syncState()
  },

  /** 提示 */
  onHint () {
    if (this.data.status !== 'playing') return
    const ok = sudoku.hint(this.state)
    if (!ok) {
      wx.showToast({ title: '提示次数已用完', icon: 'none' })
      return
    }
    this.syncState()
  },

  /** 光标移动 */
  onMoveCursor (e) {
    if (this.data.status !== 'playing') return
    const dx = Number(e.currentTarget.dataset.dx || 0)
    const dy = Number(e.currentTarget.dataset.dy || 0)
    sudoku.moveCursor(this.state, dx, dy)
    this.setData({ cursorRow: this.state.cursorRow, cursorCol: this.state.cursorCol })
  },

  /** 同步引擎状态到视图 */
  syncState () {
    this.setData({
      cells: this.buildCells(),
      emptyCount: this.state.emptyCount,
      hintCount: this.state.hintCount,
      cursorRow: this.state.cursorRow,
      cursorCol: this.state.cursorCol
    })

    if (this.state.isGameOver) this.onWin()
  },

  /** 通关结算：按剩余空格与提示次数评星 */
  onWin () {
    this.setData({ status: 'won' })

    const stars = this.calcStars()
    storage.setSudokuChapterStars(this.data.currentChapter, this.data.currentLevel, stars)
    achievements.evaluate({ type: 'sudoku.complete', hintCount: this.state.hintCount })

    // 本章最后一关拿到星 → 解锁下一章
    if (this.data.currentLevel === 10) {
      storage.unlockSudokuChapter(this.data.currentChapter + 1)
    }

    this.loadChapters()
    wx.vibrateShort({ type: 'medium' })
  },

  /** 星级：0 次提示 3 星，1 次 2 星，2 次以上 1 星 */
  calcStars () {
    if (this.state.hintCount >= 3) return 3
    if (this.state.hintCount >= 2) return 2
    return 1
  },

  /** 返回章节列表 */
  onBack () {
    this.setData({ phase: 'menu', currentLevel: 0 })
    this.loadChapters()
  },

  onShareAppMessage () {
    const card = share.buildSudokuShare(this.data.currentChapter, this.data.currentLevel, 3)
    return { title: card.title, path: card.path }
  }
})