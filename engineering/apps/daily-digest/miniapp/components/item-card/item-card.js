const { formatTime, truncate, SOURCE_LABELS } = require('../../utils/util')

Component({
  properties: {
    item: { type: Object, value: {} },
  },
  data: {
    sourceLabel: '',
  },
  observers: {
    'item.source': function (source) {
      this.setData({ sourceLabel: SOURCE_LABELS[source] || source })
    },
  },
  methods: {
    onTap() {
      wx.navigateTo({ url: `/pages/daily/daily?id=${this.data.item.id}` })
    },
    formatTime,
    truncate,
  },
})
