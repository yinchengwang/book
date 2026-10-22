/**
 * app.js —— Reading Radar 入口
 *
 * 启动时预热各数据域（首次访问写入种子数据），并初始化主题。
 */

const stores = require('./core/stores.js')
const theme = require('./core/theme.js')

App({
  globalData: {
    pendingCapture: null
  },

  onLaunch () {
    // 预热：触发各域首次读取，完成种子数据写入
    stores.excerpts.getAll()
    stores.knowledge.getAll()
    stores.studyProjects.getAll()
    stores.interviews.getAll()
    stores.mockInterviews.getAll()
    stores.learningPaths.getAll()
    stores.reviews.getAll()
    stores.daily.get()
    stores.fiveYearPlan.get()
    stores.interviewBank.get()
    stores.quizzes.get()
    stores.settings.get()
    stores.user.get()

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
