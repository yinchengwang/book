/**
 * 学习路径 —— 路径切换 / 步骤展开 / 子步骤勾选
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')

Page(withTheme({
  data: {
    paths: [],
    activeId: '',
    steps: []
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const all = stores.learningPaths.getAll()
    if (!all.length) {
      this.setData({ paths: [], activeId: '', steps: [] })
      return
    }
    const activeId = stores.learningPaths.find(this.data.activeId) ? this.data.activeId : all[0].id
    const active = stores.learningPaths.find(activeId)

    const paths = all.map(function (p) {
      const prog = stores.learningPaths.getPathProgress(p.id)
      return {
        id: p.id, name: p.name, icon: p.icon,
        difficulty: p.difficulty,
        percent: prog.percent,
        done: prog.done, total: prog.total
      }
    })

    const steps = (active.steps || []).map(function (s, i) {
      const prog = stores.learningPaths.getStepProgress(activeId, s.id)
      return {
        index: i,
        id: s.id,
        title: s.title,
        description: s.description,
        difficulty: s.difficulty,
        estimatedHours: s.estimatedHours,
        percent: prog.percent,
        done: prog.done,
        total: prog.total,
        resources: s.resources || [],
        subSteps: (s.subSteps || []).map(function (ss) {
          const meta = require('../../../core/kh-constants.js').subStepType(ss.type)
          return {
            id: ss.id,
            title: ss.title,
            typeLabel: meta.label,
            typeColor: meta.color,
            hours: ss.estimatedHours,
            done: !!active.completedSubSteps[s.id + ':' + ss.id]
          }
        })
      }
    })

    this.setData({ paths: paths, activeId: activeId, steps: steps })
  },

  onPickPath (e) {
    this.setData({ activeId: e.currentTarget.dataset.id })
    this.refresh()
  },

  onToggleSub (e) {
    const ds = e.currentTarget.dataset
    stores.learningPaths.toggleSubStep(this.data.activeId, ds.step, ds.sub)
    this.refresh()
  },

  onReset () {
    wx.showModal({
      title: '重置进度',
      content: '将清空该路径的所有子步骤完成记录',
      success: (res) => {
        if (!res.confirm) return
        stores.learningPaths.resetPath(this.data.activeId)
        this.refresh()
      }
    })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 学习路径', path: '/pages/learn/path/index' }
  }
}))