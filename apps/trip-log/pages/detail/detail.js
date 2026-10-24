// pages/detail/detail.js — 详情
const { getCloud } = require('../../utils/cloud.js')
const { ymdPretty } = require('../../utils/format.js')

Page({
  data: {
    trip: null,
    loading: true
  },

  onLoad(q) {
    this.id = q.id
    if (this.id) this.load()
  },

  async load() {
    this.setData({ loading: true })
    const cloud = await getCloud()
    const db = cloud.database()

    let res
    try {
      res = await db.collection('trips').doc(this.id).get()
    } catch (err) {
      console.error('[detail] error:', err)
      this.setData({ loading: false, trip: null })
      return
    }

    const t = res.data || {}
    if (!t._id) { this.setData({ loading: false, trip: null }); return }

    const photos = Array.isArray(t.photos) ? t.photos : []
    let photoUrls = []
    if (photos.length > 0) {
      try {
        const r = await cloud.storage().getDownloadURL({ fileIdList: photos })
        const map = r.fileList || []
        photoUrls = photos.map((id) => {
          const m = map.find((x) => x.fileID === id)
          return { url: (m && m.downloadUrl) || '' }
        }).filter((p) => p.url)
      } catch (e) {
        console.warn('[detail] getDownloadURL:', e)
      }
    }

    const tags = Array.isArray(t.tags) ? t.tags : []

    this.setData({
      loading: false,
      trip: {
        id: t._id,
        title: t.title || '',
        dateText: ymdPretty(t.date),
        location: t.location || '',
        weather: t.weather || '',
        companions: t.companions || '',
        notes: t.notes || '',
        tags,
        tagsText: tags.join(' '),
        photos: photoUrls
      }
    })
  },

  previewImg(e) {
    const urls = this.data.trip.photos.map((p) => p.url)
    wx.previewImage({ current: e.currentTarget.dataset.url, urls })
  },

  deleteTrip() {
    wx.showModal({
      title: '删除记录',
      content: '确定要删除这条足迹吗？照片也会一并删除。',
      confirmText: '删除',
      confirmColor: '#d33',
      success: async (r) => {
        if (!r.confirm) return
        wx.showLoading({ title: '删除中…', mask: true })
        try {
          const cloud = await getCloud()
          const photos = (this.data.trip.photos || []).map((p) => p.fileId).filter(Boolean)
          if (photos.length > 0) {
            try {
              await cloud.storage().deleteFile({ fileIdList: photos })
            } catch (e) { /* 忽略 */ }
          }
          await cloud.database().collection('trips').doc(this.id).remove()
          wx.hideLoading()
          wx.showToast({ title: '已删除', icon: 'success' })
          setTimeout(() => wx.navigateBack(), 500)
        } catch (err) {
          wx.hideLoading()
          wx.showToast({ title: '删除失败', icon: 'none' })
        }
      }
    })
  }
})
