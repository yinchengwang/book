/**
 * fab —— 悬浮新建按钮
 *
 * 属性：bottom 距底部距离(rpx) / label 可选文字（为空则只显示加号）
 * 事件：tap
 */
Component({
  properties: {
    bottom: { type: Number, value: 48 },
    label: { type: String, value: '' }
  },

  methods: {
    onTap () {
      this.triggerEvent('tap')
    }
  }
})
