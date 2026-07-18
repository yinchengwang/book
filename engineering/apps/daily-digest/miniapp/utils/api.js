const app = getApp()

const request = (url, data = {}, method = 'GET') => {
  return new Promise((resolve, reject) => {
    wx.request({
      url: app.globalData.baseUrl + url,
      data,
      method,
      header: { 'Content-Type': 'application/json' },
      success: (res) => {
        if (res.statusCode === 200) {
          resolve(res.data)
        } else {
          reject(new Error(res.data?.detail || '请求失败'))
        }
      },
      fail: reject,
    })
  })
}

module.exports = {
  getDaily(category = '', page = 1, size = 20) {
    return request('/daily', { category, page, size })
  },
  getItem(id) {
    return request(`/daily/${id}`)
  },
  search(q, category = '', page = 1, size = 20) {
    return request('/search', { q, category, page, size })
  },
  getCollections(userId) {
    return request('/collections', { user_id: userId })
  },
  createCollection(userId, name, description = '') {
    return request('/collections', { user_id: userId, name, description }, 'POST')
  },
  deleteCollection(id) {
    return request(`/collections/${id}`, {}, 'DELETE')
  },
  getSubscriptions(userId) {
    return request('/subscriptions', { user_id: userId })
  },
  getStats() {
    return request('/stats')
  },
}
