/**
 * app.js —— 游戏中心入口
 */

const storage = require('./utils/storage.js')

App({
  globalData: {
    /** 成就解锁广播：成就弹窗组件监听此字段 */
    achievementQueue: []
  },

  onLaunch () {
    // 触发版本迁移，初始化各游戏存储结构
    storage.migrate()
  },

  /** 供页面调用：分发成就解锁事件 */
  emitAchievement (achievement) {
    if (!achievement) return
    this.globalData.achievementQueue.push(achievement)
  },

  /** 页面 onShow 时拉取待展示成就 */
  takeAchievement () {
    if (this.globalData.achievementQueue.length === 0) return null
    return this.globalData.achievementQueue.shift()
  }
})