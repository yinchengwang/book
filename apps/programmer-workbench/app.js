/**
 * app.js —— DevBench 入口
 *
 * 启动时预热各数据域（首次访问会写入种子数据），并初始化主题。
 */

const stores = require('./core/stores.js')
const theme = require('./core/theme.js')

App({
  globalData: {
    /** 捕获浮层待处理的草稿类型，由 tab 页读取 */
    pendingCapture: null
  },

  onLaunch () {
    // 预热：触发各域首次读取，完成种子数据写入
    stores.todos.getAll()
    stores.techNotes.getAll()
    stores.snippets.getAll()
    stores.interview.getAll()
    stores.resources.getAll()
    stores.reading.getAll()
    stores.fitness.getAll()
    stores.mood.getAll()
    stores.schedule.getAll()
    stores.projects.getAll()
    stores.insights.getAll()
    stores.settings.get()

    // 首次启动标记
    const seeded = stores.settings.get()
    if (!seeded.schemaVersion) {
      stores.settings.patch({ schemaVersion: 1 })
    }
  },

  onShow () {
    // 从后台回到前台时，若为跟随系统模式则重新同步导航栏
    const settings = stores.settings.get()
    if (settings.theme === 'auto') {
      theme.syncNavBar(theme.build(settings))
    }
  }
})
