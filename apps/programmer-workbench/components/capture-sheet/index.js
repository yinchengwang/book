/**
 * capture-sheet —— 全局捕获浮层
 *
 * 「捕获优先于整理」的落地：任意内容 tab 页唤起，一步新建待办 / 闪念 / 笔记。
 *
 * 待办与闪念是短内容，用 wx.showModal 的 editable 直接输入；
 * 笔记是长内容，跳转到完整编辑器（带 Markdown 预览），不在浮层里凑合。
 *
 * 属性：visible
 * 事件：close / created（detail.type = todo|insight）
 */

const stores = require('../../core/stores.js')
const dateUtil = require('../../core/date.js')

Component({
  properties: {
    visible: { type: Boolean, value: false }
  },

  methods: {
    onClose () {
      this.triggerEvent('close')
    },

    /** 阻止点击面板内部时冒泡关闭 */
    noop () {},

    onPick (e) {
      const type = e.currentTarget.dataset.type
      const self = this

      if (type === 'note') {
        this.triggerEvent('close')
        wx.navigateTo({ url: '/pages/tech/notes/detail' })
        return
      }

      if (type === 'todo') {
        wx.showModal({
          title: '记一条待办',
          editable: true,
          placeholderText: '要做什么？',
          success: function (res) {
            if (!res.confirm) return
            const title = String(res.content || '').trim()
            if (!title) return
            stores.todos.add({
              title: title,
              done: false,
              priority: 'med',
              category: 'work',
              dueDate: dateUtil.todayStr()
            })
            self.triggerEvent('created', { type: 'todo' })
            self.triggerEvent('close')
            wx.showToast({ title: '已记下', icon: 'success' })
          }
        })
        return
      }

      if (type === 'insight') {
        wx.showModal({
          title: '记一条闪念',
          editable: true,
          placeholderText: '此刻的想法…',
          success: function (res) {
            if (!res.confirm) return
            const text = String(res.content || '').trim()
            if (!text) return
            stores.insights.add({ content: text, tags: [], pinned: false, archived: false })
            self.triggerEvent('created', { type: 'insight' })
            self.triggerEvent('close')
            wx.showToast({ title: '已记下', icon: 'success' })
          }
        })
      }
    }
  }
})
