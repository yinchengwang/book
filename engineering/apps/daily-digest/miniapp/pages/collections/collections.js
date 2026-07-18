const api = require('../../utils/api')

Page({
  data: {
    collections: [],
    newName: '',
  },

  onLoad() {
    this.loadCollections()
  },

  async loadCollections() {
    try {
      const cols = await api.getCollections(1)
      this.setData({ collections: cols || [] })
    } catch (e) {
      console.error(e)
    }
  },

  onNameInput(e) {
    this.setData({ newName: e.detail.value })
  },

  async handleCreate() {
    if (!this.data.newName.trim()) return
    try {
      await api.createCollection(1, this.data.newName)
      this.setData({ newName: '' })
      this.loadCollections()
    } catch (e) {
      console.error(e)
    }
  },
})
