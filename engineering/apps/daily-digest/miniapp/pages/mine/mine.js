Page({
  data: {
    currentTheme: 'light',
  },

  onLoad() {
    const theme = wx.getStorageSync('daily-digest-theme') || 'light'
    this.setData({ currentTheme: theme })
  },

  setTheme(e) {
    const theme = e.currentTarget.dataset.theme
    this.setData({ currentTheme: theme })
    wx.setStorageSync('daily-digest-theme', theme)
  },

  goSubscriptions() {
    wx.showToast({ title: '开发中', icon: 'none' })
  },

  goHistory() {
    wx.showToast({ title: '开发中', icon: 'none' })
  },
})
