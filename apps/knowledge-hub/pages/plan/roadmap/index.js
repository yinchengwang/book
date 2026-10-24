/**
 * 项目路线 —— 学习项目追踪（study_projects）
 *
 * 注意：这里的 project 是「学习项目」，与 programmer-workbench 的
 * 作品集 projects 同名异义，故存储键为 db:study_projects。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const khc = require('../../../core/kh-constants.js')

Page(withTheme({
  data: {
    statuses: khc.PROJECT_STATUSES,
    priorities: khc.PROJECT_PRIORITIES,
    filter: '',
    list: [],
    stats: null,
    current: null,
    showAdd: false,
    form: { name: '', description: '', status: '规划中', priority: '中', tags: '' }
  },

  onLoad () {
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const all = stores.studyProjects.getAll()
    const filter = this.data.filter
    const list = (filter ? all.filter(function (p) { return p.status === filter }) : all)
      .map(function (p) {
        const st = khc.projectStatus(p.status)
        const pr = khc.projectPriority(p.priority)
        return {
          id: p.id,
          name: p.name,
          description: p.description,
          status: p.status,
          statusLabel: st.label,
          statusColor: st.color,
          priority: p.priority,
          priorityLabel: pr.label,
          priorityColor: pr.color,
          tags: p.tags || [],
          knowledgeLinks: p.knowledgeLinks || []
        }
      })

    const stats = {
      total: all.length,
      doing: all.filter(function (p) { return p.status === '进行中' }).length,
      done: all.filter(function (p) { return p.status === '已完成' }).length
    }

    const keep = this.data.current ? stores.studyProjects.find(this.data.current.id) : null
    this.setData({ list: list, stats: stats, current: keep ? this.toDetail(keep) : null })
  },

  toDetail (p) {
    const st = khc.projectStatus(p.status)
    const pr = khc.projectPriority(p.priority)
    const nodes = stores.knowledge.getAll()
    return {
      id: p.id,
      name: p.name,
      description: p.description,
      status: p.status,
      statusLabel: st.label,
      statusColor: st.color,
      priority: p.priority,
      priorityLabel: pr.label,
      priorityColor: pr.color,
      tags: p.tags || [],
      knowledgeLinks: (p.knowledgeLinks || []).map(function (kid) {
        const n = nodes.filter(function (x) { return x.id === kid })[0]
        return { id: kid, name: n ? n.name : kid }
      }),
      createdAt: p.createdAt ? p.createdAt.slice(0, 10) : '',
      updatedAt: p.updatedAt ? p.updatedAt.slice(0, 10) : ''
    }
  },

  onPickFilter (e) {
    const s = e.currentTarget.dataset.status
    this.setData({ filter: this.data.filter === s ? '' : s })
    this.refresh()
  },

  onOpen (e) {
    const p = stores.studyProjects.find(e.currentTarget.dataset.id)
    if (p) this.setData({ current: this.toDetail(p) })
  },

  onClose () {
    this.setData({ current: null })
  },

  onChangeStatus (e) {
    const id = (e.currentTarget.dataset.id) || (this.data.current && this.data.current.id)
    const status = e.currentTarget.dataset.status
    if (!id) return
    stores.studyProjects.updateProject(id, { status: status })
    this.refresh()
  },

  onDelete () {
    const id = (e.currentTarget.dataset.id) || (this.data.current && this.data.current.id)
    if (!id) return
    wx.showModal({
      title: '删除项目',
      content: '确定删除这个学习项目？',
      success: (res) => {
        if (!res.confirm) return
        stores.studyProjects.remove(id)
        this.setData({ current: null })
        this.refresh()
      }
    })
  },

  onOpenAdd () {
    this.setData({ showAdd: true, form: { name: '', description: '', status: '规划中', priority: '中', tags: '' } })
  },

  onCloseAdd () {
    this.setData({ showAdd: false })
  },

  onAddInput (e) {
    this.setData({ ['form.' + e.currentTarget.dataset.field]: e.detail.value })
  },

  onPickForm (e) {
    const ds = e.currentTarget.dataset
    this.setData({ ['form.' + ds.field]: ds.value })
  },

  onSave () {
    const f = this.data.form
    if (!f.name.trim()) {
      wx.showToast({ title: '请填写项目名', icon: 'none' })
      return
    }
    stores.studyProjects.addProject({
      name: f.name.trim(),
      description: f.description.trim(),
      status: f.status,
      priority: f.priority,
      tags: f.tags.split(',').map(function (t) { return t.trim() }).filter(Boolean)
    })
    this.setData({ showAdd: false })
    this.refresh()
    wx.showToast({ title: '已添加', icon: 'none' })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 项目路线', path: '/pages/plan/roadmap/index' }
  }
}))