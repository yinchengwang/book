/**
 * tag —— 标签 / 徽标
 *
 * 属性：text 文本 / type 语义（default|brand|danger|warning|success）/ color 自定义文字色
 * color 优先于 type，用于分类色等动态场景。
 */
Component({
  properties: {
    text: { type: String, value: '' },
    type: { type: String, value: 'default' },
    color: { type: String, value: '' }
  }
})
