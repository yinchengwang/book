const api = require('../../utils/api')

Page({
  data: {
    query: '',
    items: [],
    page: 1,
    pageSize: 20,
    loading: false,
    searched: false,
    activeCategory: '',
    categoryTabs: [
      { key: '', label: '全部' },
      { key: 'db', label: '数据库' },
      { key: 'ai', label: 'AI' },
      { key: 'ml', label: 'ML' },
      { key: 'infra', label: '基础设施' },
    ],
  },

  onQueryInput(e) {
    this.setData({ query: e.detail.value })
  },

  async doSearch() {
    if (!this.data.query.trim()) return
    this.setData({ loading: true, searched: true, page: 1 })
    try {
      const res = await api.search(this.data.query, this.data.activeCategory)
      this.setData({ items: res.items || [], loading: false })
    } catch (e) {
      console.error(e)
      this.setData({ loading: false })
    }
  },

  onCategoryChange(e) {
    this.setData({ activeCategory: e.detail.key })
    if (this.data.query.trim()) this.doSearch()
  },
})
