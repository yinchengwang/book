/**
 * 首页 —— 游戏中心
 */

const storage = require('../../utils/storage.js')

Page({
  data: {
    games: [],
    achievements: [],
    achievementUnlocked: 0,
    achievementTotal: 0
  },

  onShow () {
    this.refresh()
  },

  /** 刷新汇总数据 */
  refresh () {
    const overview = storage.getOverview()

    this.setData({
      games: [
        {
          id: 'snake',
          title: '贪吃蛇',
          desc: '经典贪吃蛇 · 越吃越快',
          emoji: '🐍',
          color: 'linear-gradient(135deg, #11998e, #38ef7d)',
          stat: '最高 ' + overview.snakeBest + ' 分'
        },
        {
          id: 'game2048',
          title: '2048',
          desc: '数字合成 · 挑战 2048',
          emoji: '🔢',
          color: 'linear-gradient(135deg, #f7971e, #ffd200)',
          stat: '最高 ' + overview.g2048Best + ' 分'
        },
        {
          id: 'sudoku',
          title: '数独',
          desc: '五章关卡 · 逻辑推理',
          emoji: '🔲',
          color: 'linear-gradient(135deg, #667eea, #764ba2)',
          stat: '已获 ' + overview.sudokuStars + ' 星'
        },
        {
          id: 'match3',
          title: '消消乐',
          desc: '六章 50 关 · 特效消除',
          emoji: '💎',
          color: 'linear-gradient(135deg, #ee0979, #ff6a00)',
          stat: '已获 ' + overview.match3Stars + ' 星'
        }
      ],
      achievementUnlocked: overview.achievementUnlocked,
      achievementTotal: 8
    })
  },

  /** 进入游戏 */
  onGameTap (e) {
    const id = e.currentTarget.dataset.id
    wx.navigateTo({ url: '/pages/' + id + '/index' })
  },

  /** 切换成就墙展开 */
  onToggleAchievements () {
    if (this.data.achievements.length > 0) {
      this.setData({ achievements: [] })
      return
    }
    // require 延迟加载，避免首屏多余开销
    const achievements = require('../../utils/achievements.js')
    this.setData({ achievements: achievements.getAchievementList() })
  },

  /** 确认弹窗 */
  onResetConfirm () {
    const self = this
    wx.showModal({
      title: '清空所有数据',
      content: '将删除全部最高分、关卡进度与成就，且不可恢复。确认继续？',
      confirmText: '确认清空',
      confirmColor: '#e53e3e',
      success (res) {
        if (!res.confirm) return
        storage.resetAll()
        self.refresh()
        self.setData({ achievements: [] })
        wx.showToast({ title: '已清空', icon: 'success' })
      }
    })
  },

  onShareAppMessage () {
    return { title: '游戏中心 · 四款经典小游戏', path: '/pages/index/index' }
  }
})