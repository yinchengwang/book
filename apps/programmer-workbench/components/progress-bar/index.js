/**
 * progress-bar —— 进度条
 *
 * 属性：percent 0-100 / color 自定义颜色 / height 高度(rpx)
 */
Component({
  properties: {
    percent: { type: Number, value: 0 },
    color: { type: String, value: '' },
    height: { type: Number, value: 8 }
  },

  data: {
    safePercent: 0
  },

  observers: {
    percent: function (value) {
      const n = Number(value)
      const safe = isNaN(n) ? 0 : Math.max(0, Math.min(100, n))
      this.setData({ safePercent: safe })
    }
  }
})
