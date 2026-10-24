// pages/index/index.js — 时间线主页
const { getCloud, isConfigured } = require('../../utils/cloud.js')
const { ymdPretty, groupKey, groupLabel } = require('../../utils/format.js')

Page({
  data: {
    loading: true,
    trips: [],
    grouped: [],
    cloudReady: isConfigured(),
    cloudHint: ''
  },

  onLoad() {
    this.load()
  },

  onShow() {
    this.load()
  },

  onPullDownRefresh() {
    this.load().finally(() => wx.stopPullDownRefresh())
  },

  async load() {
    // 把 cloud 状态实时刷到 UI
    this.setData({ cloudReady: isConfigured() })

    if (!isConfigured()) {
      // 未开通云服务，直接给一个提示，page 不会因为这个黑屏
      this.setData({
        loading: false,
        cloudHint: '云服务尚未开通。新记录暂时无法保存。',
        trips: [],
        grouped: []
      })
      return
    }

    const app = getApp()
    if (!app.globalData.isLoginReady) {
      await new Promise((r) => {
        if (app.globalData.isLoginReady) return r()
        const t0 = Date.now()
        const iv = setInterval(() => {
          if (app.globalData.isLoginReady || Date.now() - t0 > 3000) {
            clearInterval(iv); r()
          }
        }, 100)
      })
    }

    let res
    try {
      const cloud = await getCloud()
      const db = cloud.database()
      res = await db.collection('trips')
        .where({ _openid: app.globalData.openid })
        .orderBy('date', 'desc')
        .limit(200)
        .get()
    } catch (err) {
      console.error('[list] error:', err)
      this.setData({
        loading: false,
        trips: [],
        grouped: [],
        cloudHint: '载入失败：' + (err.message || err.errMsg || '请重试')
      })
      return
    }

    try {
      const storage = (await getCloud()).storage()
      const tripsRaw = res.data || []
      const trips = []
      for (const t of tripsRaw) {
        const photos = Array.isArray(t.photos) ? t.photos : []
        let cover = ''
        let photoCount = photos.length
        if (photoCount > 0) {
          try {
            const r = await storage.getDownloadURL({ fileId: photos[0] })
            cover = (r.fileList && r.fileList[0] && r.fileList[0].downloadUrl) || r.downloadUrl || ''
          } catch (e) { /* skip cover */ }
        }
        trips.push({
          id: t._id,
          title: t.title || '',
          location: t.location || '',
          weather: t.weather || '',
          dateText: ymdPretty(t.date),
          notes: t.notes || '',
          tags: Array.isArray(t.tags) ? t.tags : [],
          tagsText: (Array.isArray(t.tags) ? t.tags.join(' ') : ''),
          cover,
          photoCount
        })
      }

      const map = new Map()
      for (const t of trips) {
        const iso = tripsRaw.find((x) => x._id === t.id).date || ''
        const k = groupKey(iso)
        if (!map.has(k)) map.set(k, { key: k, label: groupLabel(iso), items: [] })
        map.get(k).items.push(t)
      }
      const grouped = Array.from(map.values())

      this.setData({ loading: false, trips, grouped, cloudHint: '' })
    } catch (err) {
      console.error('[list] enrich error:', err)
      this.setData({ loading: false, cloudHint: '照片载入失败：' + (err.message || '') })
    }
  },

  addTrip() {
    wx.navigateTo({ url: '/pages/new-trip/new-trip' })
  },

  openTrip(e) {
    const id = e.currentTarget.dataset.id
    wx.navigateTo({ url: '/pages/detail/detail?id=' + id })
  }
})
