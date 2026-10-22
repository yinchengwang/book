/**
 * card —— 通用卡片
 *
 * 属性：title 标题 / extra 右上角附加文字 / tight 紧凑内距 / flat 更小圆角
 * 插槽：默认插槽放内容
 */
Component({
  options: {
    multipleSlots: true
  },

  properties: {
    title: { type: String, value: '' },
    extra: { type: String, value: '' },
    tight: { type: Boolean, value: false },
    flat: { type: Boolean, value: false }
  }
})
