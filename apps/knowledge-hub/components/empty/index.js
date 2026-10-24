/**
 * empty —— 空状态
 *
 * 属性：text 说明文字 / actionText 主操作文案（为空则不显示按钮）
 * 事件：action
 */
Component({
  properties: {
    text: { type: String, value: '还没有内容' },
    actionText: { type: String, value: '' }
  },

  methods: {
    onAction () {
      this.triggerEvent('action')
    }
  }
})
