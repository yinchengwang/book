// pages/new-trip/new-trip.js — 新增记录
const { getCloud } = require('../../utils/cloud.js')
const { todayISO } = require('../../utils/format.js')

const MAX_PHOTOS = 9
const MAX_PHOTO_BYTES = 5 * 1024 * 1024

Page({
  data: {
    form: {
      title: '',
      date: '',
      weather: '',
      location: '',
      companions: '',
      tagsText: '',
      notes: ''
    },
    localPhotos: [],
    saving: false
  },

  onLoad() {
    this.setData({ 'form.date': todayISO() })
  },

  onInput(e) {
    const field = e.currentTarget.dataset.field
    this.setData({ ['form.' + field]: e.detail.value })
  },

  onDate(e) {
    this.setData({ 'form.date': e.detail.value })
  },

  pickPhotos() {
    const remain = MAX_PHOTOS - this.data.localPhotos.length
    if (remain <= 0) {
      wx.showToast({ title: '最多 ' + MAX_PHOTOS + ' 张', icon: 'none' })
      return
    }
    wx.chooseMedia({
      count: remain,
      mediaType: ['image'],
      sourceType: ['album', 'camera'],
      sizeType: ['compressed'],
      success: (res) => {
        const files = (res.tempFiles || []).map((f) => ({
          path: f.tempFilePath,
          size: f.size
        })).filter((f) => f.size <= MAX_PHOTO_BYTES)
        if (files.length === 0) {
          wx.showToast({ title: '照片超过 5MB 限制', icon: 'none' })
          return
        }
        this.setData({ localPhotos: this.data.localPhotos.concat(files) })
      },
      fail: () => {}
    })
  },

  removePhoto(e) {
    const path = e.currentTarget.dataset.path
    this.setData({ localPhotos: this.data.localPhotos.filter((p) => p.path !== path) })
  },

  previewPhoto(e) {
    const path = e.currentTarget.dataset.path
    const urls = this.data.localPhotos.map((p) => p.path)
    wx.previewImage({ current: path, urls })
  },

  cancel() {
    wx.navigateBack()
  },

  async save() {
    const f = this.data.form
    if (!f.title || !f.date) {
      wx.showToast({ title: '请填写标题和日期', icon: 'none' })
      return
    }

    this.setData({ saving: true })
    wx.showLoading({ title: '上传中…', mask: true })

    try {
      const cloud = await getCloud()
      const openid = getApp().globalData.openid

      const storage = cloud.storage()
      const uploadedIds = []
      for (let i = 0; i < this.data.localPhotos.length; i++) {
        const ph = this.data.localPhotos[i]
        const cloudPath = 'trips/' + openid + '/' + Date.now() + '_' + i + '.jpg'
        const r = await storage.uploadFile({ cloudPath, filePath: ph.path })
        uploadedIds.push(r.fileID)
      }

      const tags = (f.tagsText || '').split(/[,，]/)
        .map((s) => s.trim())
        .filter(Boolean)

      const db = cloud.database()
      await db.collection('trips').add({
        data: {
          title: f.title.trim(),
          date: f.date,
          weather: (f.weather || '').trim(),
          location: (f.location || '').trim(),
          companions: (f.companions || '').trim(),
          tags,
          notes: (f.notes || '').trim(),
          photos: uploadedIds,
          createdAt: Date.now()
        }
      })

      wx.hideLoading()
      wx.showToast({ title: '已保存', icon: 'success' })
      setTimeout(() => wx.navigateBack(), 600)
    } catch (err) {
      console.error('[save] error:', err)
      wx.hideLoading()
      wx.showToast({ title: '保存失败：' + (err.errMsg || err.message || '未知错误'), icon: 'none' })
      this.setData({ saving: false })
    }
  }
})
