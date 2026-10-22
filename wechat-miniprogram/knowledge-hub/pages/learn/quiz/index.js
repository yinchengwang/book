/**
 * 题库练习 —— 5 种模式
 *
 * plan  今日计划（按 stack 轮换生成）
 * list  题目列表（按栈筛选 / 搜索）
 * quiz  答题（选择 / 判断 / 填空 / 简答）
 * wrong 错题本
 * stats 统计与各栈进度
 */

const { withTheme } = require('../../../core/with-theme.js')
const stores = require('../../../core/stores.js')
const khc = require('../../../core/kh-constants.js')

Page(withTheme({
  data: {
    mode: 'plan',
    stacks: [],
    activeStack: '',
    query: '',
    // plan
    plan: null,
    planPercent: 0,
    // list
    questions: [],
    // quiz
    session: null,
    index: 0,
    picked: '',
    result: null,
    // wrong
    wrongs: [],
    // stats
    stats: null,
    progress: []
  },

  onLoad () {
    this.setData({ stacks: khc.STACK_META })
    this.refresh()
  },

  onShow () {
    this.refresh()
  },

  refresh () {
    const plan = stores.quizzes.getTodayPlan()
    const stats = stores.quizzes.getStats()

    this.setData({
      plan: {
        stack: plan.stack,
        stackLabel: khc.stackMeta(plan.stack).label,
        done: plan.completed || 0,
        total: plan.questionIds.length
      },
      planPercent: plan.questionIds.length ? Math.round((plan.completed || 0) / plan.questionIds.length * 100) : 0,
      questions: this.filteredQuestions(),
      wrongs: stores.quizzes.getWrongBook(),
      stats: {
        totalAnswered: stats.totalAnswered,
        totalCorrect: stats.totalCorrect,
        streak: stats.streak,
        accuracy: stats.accuracy,
        accuracyPct: Math.round(stats.accuracy * 100)
      },
      progress: khc.STACK_META.map(function (s) {
        const p = stores.quizzes.getProgress(s.key)
        return {
          key: s.key, label: s.label, color: s.color,
          total: p.total, answered: p.answered, correct: p.correct,
          percent: p.total ? Math.round(p.answered / p.total * 100) : 0
        }
      })
    })
  },

  filteredQuestions () {
    const stack = this.data.activeStack
    const q = this.data.query
    let list = q ? stores.quizzes.searchQuestions(q) : stores.quizzes.getQuestionBank()
    if (stack) list = list.filter(function (item) { return item.stack === stack })
    return list.map(function (item) {
      const meta = khc.stackMeta(item.stack)
      const diff = khc.quizDifficulty(item.difficulty)
      return {
        id: item.id,
        stem: item.stem,
        scenario: item.scenario,
        stackLabel: meta.label,
        stackColor: meta.color,
        difficulty: diff.label,
        difficultyColor: diff.color,
        type: item.type
      }
    })
  },

  /* ---- 模式切换 ---- */
  onMode (e) {
    this.setData({ mode: e.currentTarget.dataset.mode })
    this.refresh()
  },

  onPickStack (e) {
    const key = e.currentTarget.dataset.key
    this.setData({ activeStack: this.data.activeStack === key ? '' : key })
    this.setData({ questions: this.filteredQuestions() })
  },

  onSearch (e) {
    this.setData({ query: e.detail.value })
    this.setData({ questions: this.filteredQuestions() })
  },

  /* ---- 开始会话 ---- */
  onStartPlan () {
    const plan = stores.quizzes.getTodayPlan()
    this.beginSession(plan.questionIds, 'plan')
  },

  onRegeneratePlan () {
    stores.quizzes.generateDailyPlan()
    this.refresh()
    wx.showToast({ title: '已换一批', icon: 'none' })
  },

  onStartStack (e) {
    const key = e.currentTarget.dataset.key
    const ids = stores.quizzes.getQuestionBank()
      .filter(function (q) { return q.stack === key })
      .slice(0, 20)
      .map(function (q) { return q.id })
    if (!ids.length) {
      wx.showToast({ title: '该栈暂无题目', icon: 'none' })
      return
    }
    this.beginSession(ids, 'list')
  },

  onStartWrong () {
    const ids = this.data.wrongs.slice(0, 10).map(function (w) { return w.questionId })
    if (!ids.length) {
      wx.showToast({ title: '错题本是空的', icon: 'none' })
      return
    }
    this.beginSession(ids, 'wrong')
  },

  onAnswerOne (e) {
    this.beginSession([e.currentTarget.dataset.id], 'list')
  },

  beginSession (ids, mode) {
    const qs = ids.map(function (id) { return stores.quizzes.getQuestionById(id) }).filter(Boolean)
    this.setData({ mode: 'quiz', session: qs, index: 0, picked: '', result: null })
    this.renderQuestion()
  },

  /* ---- 答题 ---- */
  renderQuestion () {
    const q = this.data.session[this.data.index]
    if (!q) return
    const diff = khc.quizDifficulty(q.difficulty)
    const meta = khc.stackMeta(q.stack)

    // 选择题 / 判断题渲染成可点选项
    let options = []
    if (q.type === '选择题' && q.options) {
      options = q.options.map(function (text, i) {
        return { text: text, letter: String.fromCharCode(65 + i) }
      })
    } else if (q.type === '判断题') {
      options = [{ text: '正确', letter: 'T' }, { text: '错误', letter: 'F' }]
    }

    this.setData({
      picked: '',
      result: null,
      q: {
        id: q.id,
        type: q.type,
        stem: q.stem,
        scenario: q.scenario,
        code: q.code || '',
        stackLabel: meta.label,
        difficulty: diff.label,
        difficultyColor: diff.color,
        answerText: q.type === '选择题' ? q.answer : '',
        explain: q.explanation || '',
        position: (this.data.index + 1) + ' / ' + this.data.session.length
      },
      options: options
    })
  },

  onPickOption (e) {
    if (this.data.result) return
    this.setData({ picked: e.currentTarget.dataset.letter })
  },

  onSubmit () {
    const q = this.data.session[this.data.index]
    if (!q) return
    const picked = this.data.picked
    if (!picked) {
      wx.showToast({ title: '先选择一个答案', icon: 'none' })
      return
    }

    const rec = stores.quizzes.submitAnswer({ questionId: q.id, userAnswer: picked })
    stores.daily.recordStudy(q.id, rec.correct)
    if (this.data.mode === 'plan') stores.quizzes.markPlanProgress(q.id)

    this.setData({
      result: {
        correct: rec.correct,
        right: q.type === '判断题' ? (q.answer ? '正确' : '错误') : q.answer,
        explain: q.explanation || ''
      }
    })
    wx.vibrateShort({ type: rec.correct ? 'medium' : 'heavy' })
  },

  onNext () {
    const next = this.data.index + 1
    if (next >= this.data.session.length) {
      this.setData({ mode: this.data.mode === 'quiz' ? (this.data.session.length ? 'plan' : 'plan') : 'plan', session: null })
      this.refresh()
      wx.showToast({ title: '本轮完成', icon: 'none' })
      return
    }
    this.setData({ index: next })
    this.renderQuestion()
  },

  onExitQuiz () {
    this.setData({ mode: 'plan', session: null, index: 0 })
    this.refresh()
  },

  onShareAppMessage () {
    return { title: 'Reading Radar · 题库练习', path: '/pages/learn/quiz/index' }
  }
}))