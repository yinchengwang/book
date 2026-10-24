/**
 * 模拟面试 —— 配置 → 答题（倒计时）→ 评分报告 → 历史
 *
 * 评分为本地规则算法（原版为 mock，未接真实 AI）：
 *   字数 < 20 → -20；无结构 → -10；否则按字数分档 90/75/60。
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const seedData = require('../../../core/seed-data.js')
const date = require('../../../core/date.js')

const ROLES = ['后端开发', '前端开发', '算法工程师', '全栈工程师']
const LEVELS = ['初级', '中级', '高级']

/** 本地规则评分（逐字对齐数据模型清单 C-9） */
function evaluate (answer) {
  const text = String(answer || '').trim()
  let score = 100
  const suggestions = []

  if (text.length < 20) {
    score -= 20
    suggestions.push('回答过于简短，补充具体细节')
  }
  // 结构化信号：编号、分点、关键词连接
  const hasStructure = /\d+[.、)]|[•\-*]|首先|其次|然后|最后|综上/.test(text)
  if (!hasStructure) {
    score -= 10
    suggestions.push('建议分点作答，结构更清晰')
  }

  if (score >= 90) score = 90
  else if (score >= 75) score = 75
  else if (score >= 60) score = 60
  else score = Math.max(score, 40)

  const feedback = score >= 90
    ? '要点完整，条理清晰。'
    : (score >= 75 ? '主体正确，可再补充边界情况。' : '存在明显缺失，建议重新组织。')

  return { score: score, feedback: feedback, suggestions: suggestions }
}

Page(withTheme({
  data: {
    roles: ROLES,
    levels: LEVELS,
    config: { role: '后端开发', difficulty: '中级', minutes: 20, count: 3 },
    step: 'config',
    questions: [],
    index: 0,
    answers: [],
    remaining: 0,
    history: []
  },

  onLoad () {
    this.setData({ history: stores.mockInterviews.getAll() })
  },

  onShow () {
    this.setData({ history: stores.mockInterviews.getAll() })
  },

  onUnload () {
    this.stopTimer()
  },

  onPick (e) {
    const ds = e.currentTarget.dataset
    const field = ds.field
    const value = ds.type === 'number' ? Number(ds.value) : ds.value
    this.setData({ ['config.' + field]: value })
  },

  /** 从面试题库按岗位抽题 */
  buildQuestions () {
    const cfg = this.data.config
    const pool = seedData.INTERVIEW_BANK.interview.concat(seedData.INTERVIEW_BANK.algorithm)
    const levelIdx = LEVELS.indexOf(cfg.difficulty)
    // 难度越高级别越靠后
    const ordered = pool.slice().sort(function (a, b) {
      const ai = LEVELS.indexOf(a.difficulty)
      const bi = LEVELS.indexOf(b.difficulty)
      return (ai >= levelIdx ? 1 : 0) - (bi >= levelIdx ? 1 : 0)
    })
    return ordered.slice(0, cfg.count).map(function (q, i) {
      return {
        id: q.id,
        question: q.question,
        reference: q.answer,
        answer: ''
      }
    })
  },

  onStart () {
    const questions = this.buildQuestions()
    if (!questions.length) {
      wx.showToast({ title: '题库为空', icon: 'none' })
      return
    }
    this.setData({
      step: 'interview',
      questions: questions,
      index: 0,
      answers: questions.map(function () { return '' }),
      remaining: this.data.config.minutes * 60
    })
    this.startTimer()
  },

  startTimer () {
    const self = this
    this.stopTimer()
    this.__timer = setInterval(function () {
      const r = self.data.remaining - 1
      if (r <= 0) {
        self.stopTimer()
        self.setData({ remaining: 0 })
        self.onFinish()
        return
      }
      self.setData({ remaining: r })
    }, 1000)
  },

  stopTimer () {
    if (this.__timer) {
      clearInterval(this.__timer)
      this.__timer = null
    }
  },

  onAnswer (e) {
    const answers = this.data.answers.slice()
    answers[this.data.index] = e.detail.value
    this.setData({ answers: answers })
  },

  onNext () {
    if (this.data.index < this.data.questions.length - 1) {
      this.setData({ index: this.data.index + 1 })
    } else {
      this.onFinish()
    }
  },

  onPrev () {
    if (this.data.index > 0) this.setData({ index: this.data.index - 1 })
  },

  onFinish () {
    this.stopTimer()
    const qs = this.data.questions
    const answers = this.data.answers

    let total = 0
    const evaluated = qs.map(function (q, i) {
      const ev = evaluate(answers[i])
      total += ev.score
      return {
        question: q.question,
        answer: answers[i] || '',
        evaluation: ev
      }
    })
    const avg = Math.round(total / (qs.length || 1))

    stores.mockInterviews.addSession({
      date: date.todayStr(),
      role: this.data.config.role,
      difficulty: this.data.config.difficulty,
      questions: evaluated,
      totalScore: avg,
      duration: this.data.config.minutes
    })

    this.setData({ step: 'result', result: { score: avg, questions: evaluated } })
    this.setData({ history: stores.mockInterviews.getAll() })
  },

  onBackConfig () {
    this.stopTimer()
    this.setData({ step: 'config', result: null })
  },

  onOpenHistory (e) {
    const id = e.currentTarget.dataset.id
    const s = stores.mockInterviews.getSessionById(id)
    if (!s) return
    this.setData({ step: 'result', result: { score: s.totalScore, questions: s.questions, past: true } })
  },

  onDeleteHistory (e) {
    const id = e.currentTarget.dataset.id
    wx.showModal({
      title: '删除记录',
      content: '确定删除这场模拟面试？',
      success: (res) => {
        if (!res.confirm) return
        stores.mockInterviews.deleteSession(id)
        this.setData({ history: stores.mockInterviews.getAll() })
      }
    })
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 模拟面试', path: '/pages/interview/mock/index' }
  }
}))