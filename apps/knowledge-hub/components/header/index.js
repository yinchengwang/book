/**
 * header —— 页头（用于子页，含返回）
 *
 * 属性：title 标题 / showBack 是否显示返回 / rightText 右侧文字
 * 事件：back / right
 *
 * tab 页使用系统导航栏，不需要此组件。
 */
Component({
  options: {
    multipleSlots: true
  },

  properties: {
    title: { type: String, value: '' },
    showBack: { type: Boolean, value: false },
    rightText: { type: String, value: '' }
  },

  methods: {
    onBack () {
      this.triggerEvent('back')
      const pages = getCurrentPages()
      if (pages.length > 1) {
        wx.navigateBack()
      } else {
        wx.switchTab({ url: '/pages/dashboard/index' })
      }
    },

    onRight () {
      this.triggerEvent('right')
    }
  }
})
