const api = require('../../utils/api')
const { CATEGORY_LABELS } = require('../../utils/util')

Page({
  data: {
    items: [],
    total: 0,
    page: 1,
    pageSize: 20,
    loading: false,
    activeCategory: '',
    dateStr: '',
    categoryTabs: [
      { key: '', label: '全部' },
      { key: 'db', label: '数据库' },
      { key: 'ai', label: 'AI' },
      { key: 'ml', label: 'ML' },
      { key: 'infra', label: '基础设施' },
    ],
  },

  onLoad() {
    const now = new Date()
    this.setData({
      dateStr: `${now.getMonth() + 1}/${now.getDate()}`,
    })
    this.loadItems()
  },

  onPullDownRefresh() {
    this.setData({ page: 1 })
    this.loadItems().then(() => wx.stopPullDownRefresh())
  },

  async loadItems() {
    this.setData({ loading: true })
    try {
      const res = await api.getDaily(
        this.data.activeCategory,
        this.data.page,
        this.data.pageSize
      )
      this.setData({
        items: res.items || [],
        total: res.total || 0,
        loading: false,
      })
    } catch (e) {
      console.error('加载失败:', e)
      this.setData({ loading: false })
    }
  },

  onCategoryChange(e) {
    this.setData({
      activeCategory: e.detail.key,
      page: 1,
    })
    this.loadItems()
  },

  onReachBottom() {
    const { page, total, pageSize } = this.data
    if (page * pageSize < total) {
      this.setData({ page: page + 1 })
      this.loadItems()
    }
  },
})
