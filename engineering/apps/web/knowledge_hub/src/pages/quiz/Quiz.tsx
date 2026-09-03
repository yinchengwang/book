import { useState, useEffect, useMemo } from 'react'
import { View, Text, Button, Input, Textarea } from '@tarojs/components'
import { useQuizStore, QuizQuestion } from '@/stores/quizStore'
import { quizStats, quizByStack } from '@/data/quiz-index'
import './Quiz.scss'

const STACKS = ['c', 'cpp', 'db', 'ds', 'linux', 'py', 'vdb'] as const
type Stack = typeof STACKS[number]

const StackBadge = ({ stack, count }: { stack: string; count?: number }) => (
  <Text className={`stack-chip stack-${stack}`}>
    {stack.toUpperCase()}{count !== undefined ? ` · ${count}` : ''}
  </Text>
)

const Quiz = () => {
  const {
    questions,
    answers,
    currentSession,
    stackProgress,
    dailyPlans,
    startSession,
    submitAnswer,
    nextQuestion,
    prevQuestion,
    endSession,
    generateDailyPlan,
    getTodayPlan,
    getWrongBook,
    getStats,
    getProgress,
    getQuestionById
  } = useQuizStore()

  const [mode, setMode] = useState<'plan' | 'list' | 'quiz' | 'wrong' | 'stats'>('plan')
  const [filter, setFilter] = useState<{ stack: Stack | 'all'; category: string; difficulty: string }>({
    stack: 'all',
    category: '全部',
    difficulty: '全部'
  })
  const [userAnswer, setUserAnswer] = useState('')
  const [sessionStartTime, setSessionStartTime] = useState(0)
  // 折叠式展开：每张 list 卡片可独立折叠/展开
  const [expandedQuestions, setExpandedQuestions] = useState<Set<string>>(new Set())

  // 初始化今日计划
  const todayPlan = getTodayPlan()
  useEffect(() => {
    if (!todayPlan) {
      generateDailyPlan()
    }
  }, [])

  // 当日统计
  const stats = useMemo(() => getStats(), [answers])

  // 错题本
  const wrongAnswers = useMemo(() => getWrongBook(), [answers])

  // 筛选题目（支持按 stack / category / difficulty）
  const filteredQuestions = useMemo(() => {
    let pool = questions
    if (filter.stack !== 'all') {
      // 优先用 quizByStack 数组（更准确），否则在 questions 中按 domain 包含 stack 名匹配
      pool = (quizByStack as any)[filter.stack] || questions.filter(q =>
        q.domain?.toLowerCase().includes(filter.stack) ||
        q.tags?.includes(filter.stack)
      )
    }
    return pool.filter(q => {
      if (filter.category !== '全部' && q.category !== filter.category) return false
      if (filter.difficulty !== '全部' && q.difficulty !== filter.difficulty) return false
      return true
    })
  }, [questions, filter])

    // 筛选选项（动态从当前 pool 提取）
  const categories = useMemo(() => {
    const pool = filter.stack === 'all' ? questions : ((quizByStack as any)[filter.stack] || [])
    const set = new Set(pool.map((q: QuizQuestion) => q.category))
    return ['全部', ...Array.from(set)] as string[]
  }, [questions, filter.stack])

  const difficulties = useMemo(() => {
    const pool = filter.stack === 'all' ? questions : ((quizByStack as any)[filter.stack] || [])
    const set = new Set(pool.map((q: QuizQuestion) => q.difficulty))
    return ['全部', ...Array.from(set)] as string[]
  }, [questions, filter.stack])

  // 开始今日计划
  const startTodayPlan = () => {
    const plan = getTodayPlan() || generateDailyPlan()
    if (plan.questionIds.length === 0) {
      // 如果生成失败，从题库随机取 10 道
      const random = [...questions].sort(() => Math.random() - 0.5).slice(0, 10)
      startSession(random.map(q => q.id))
    } else {
      startSession(plan.questionIds)
    }
    setSessionStartTime(Date.now())
    setUserAnswer('')
    setMode('quiz')
  }

  // 开始列表测验
  const startListQuiz = () => {
    if (filteredQuestions.length === 0) return
    startSession(filteredQuestions.slice(0, 20).map(q => q.id))
    setSessionStartTime(Date.now())
    setUserAnswer('')
    setMode('quiz')
  }

  // 折叠式展开：单张卡片切换
  const toggleExpand = (id: string) => {
    setExpandedQuestions(prev => {
      const next = new Set(prev)
      if (next.has(id)) next.delete(id)
      else next.add(id)
      return next
    })
  }

  // 开始错题重做
  const startWrongQuiz = () => {
    const ids = wrongAnswers.slice(0, 10).map(a => a.questionId)
    if (ids.length === 0) return
    startSession(ids)
    setSessionStartTime(Date.now())
    setUserAnswer('')
    setMode('quiz')
  }

  // 提交答案
  const handleSubmit = () => {
    if (!currentSession || !userAnswer.trim()) return
    const timeSpent = Math.floor((Date.now() - sessionStartTime) / 1000)
    submitAnswer(userAnswer, timeSpent)
  }

  // 下一题
  const handleNext = () => {
    if (!currentSession) return
    if (currentSession.currentIndex < currentSession.questionIds.length - 1) {
      nextQuestion()
      setSessionStartTime(Date.now())
      setUserAnswer('')
    } else {
      endSession()
      setMode('plan')
    }
  }

  // 当前题目
  const currentQ = currentSession
    ? getQuestionById(currentSession.questionIds[currentSession.currentIndex])
    : null

  const isAnswered = currentSession?.isAnswered || false
  const lastAnswer = currentSession
    ? currentSession.answers[currentSession.currentIndex]
    : null

  // 计算当前题目是否正确（仅显示已答后）
  const lastResult = currentQ && answers.find(a => a.questionId === currentQ.id && a.timestamp >= sessionStartTime - 1000)
  const isCorrect = lastResult?.correct

  // ============ 渲染：今日计划 =============
  if (mode === 'plan') {
    const plan = getTodayPlan()
    const allProgress = getProgress()
    const currentStackProgress = plan ? getProgress(plan.stack) : null

    return (
      <View className="quiz-page plan-mode">
        {/* 头部统计 */}
        <View className="plan-header">
          <View className="welcome-card">
            <Text className="welcome-icon">📚</Text>
            <View className="welcome-content">
              <Text className="welcome-title">今日刷题</Text>
              <Text className="welcome-sub">{plan?.stack ? `今日主推: ${plan.stack.toUpperCase()}` : '准备生成今日计划'}</Text>
            </View>
          </View>

          <View className="stats-row">
            <View className="stat-item">
              <Text className="stat-num">{stats.totalAnswered}</Text>
              <Text className="stat-label">累计答题</Text>
            </View>
            <View className="stat-item">
              <Text className="stat-num">{Math.round(stats.accuracy * 100)}%</Text>
              <Text className="stat-label">正确率</Text>
            </View>
            <View className="stat-item">
              <Text className="stat-num">{stats.streak}</Text>
              <Text className="stat-label">连续天数</Text>
            </View>
            <View className="stat-item">
              <Text className="stat-num">{wrongAnswers.length}</Text>
              <Text className="stat-label">错题数</Text>
            </View>
          </View>
        </View>

        {/* 今日计划卡片 */}
        <View className="plan-card">
          <View className="plan-card-header">
            <View>
              <Text className="plan-card-title">📅 今日计划</Text>
              <Text className="plan-card-sub">完成今日 10 题，挑战自己</Text>
            </View>
            {plan && (
              <Text className="plan-progress">{plan.completed}/{plan.questionIds.length}</Text>
            )}
          </View>

          {/* 进度条 */}
          {plan && (
            <View className="progress-bar">
              <View
                className="progress-fill"
                style={{ width: `${(plan.completed / plan.questionIds.length) * 100}%` }}
              />
            </View>
          )}

          <View className="plan-actions">
            <Button className="btn-primary" onClick={startTodayPlan}>
              ▶ 开始今日计划
            </Button>
            <Button className="btn-secondary" onClick={() => generateDailyPlan()}>
              🔄 换一批
            </Button>
          </View>
        </View>

        {/* 快速入口 */}
        <View className="quick-entries">
          <View className="entry-card" onClick={() => setMode('list')}>
            <Text className="entry-icon">📝</Text>
            <Text className="entry-title">浏览题库</Text>
            <Text className="entry-desc">{questions.length} 题</Text>
          </View>
          <View className="entry-card" onClick={() => setMode('wrong')}>
            <Text className="entry-icon">❌</Text>
            <Text className="entry-title">错题本</Text>
            <Text className="entry-desc">{wrongAnswers.length} 道错题</Text>
          </View>
          <View className="entry-card" onClick={startWrongQuiz}>
            <Text className="entry-icon">🔁</Text>
            <Text className="entry-title">错题重做</Text>
            <Text className="entry-desc">巩固薄弱点</Text>
          </View>
          <View className="entry-card" onClick={() => setMode('stats')}>
            <Text className="entry-icon">📊</Text>
            <Text className="entry-title">学习统计</Text>
            <Text className="entry-desc">查看进度</Text>
          </View>
        </View>

        {/* Stack 进度 - 显示真实题数（来自 quizStats） */}
        <View className="stack-progress">
          <Text className="section-title">📈 各栈题库（{quizStats.total} 题）</Text>
          <View className="stack-grid">
            {STACKS.map(stack => {
              const total = (quizStats as any)[stack] || 0
              const answered = stackProgress[stack]?.answered || 0
              const correct = stackProgress[stack]?.correct || 0
              const percent = total > 0 ? Math.round((answered / total) * 100) : 0
              return (
                <View
                  key={stack}
                  className="stack-card"
                  onClick={() => {
                    setFilter({ ...filter, stack: stack as Stack })
                    setMode('list')
                  }}
                >
                  <View className="stack-card-head">
                    <StackBadge stack={stack} />
                    <Text className="stack-card-total">{total}</Text>
                  </View>
                  <View className="stack-bar">
                    <View className="stack-fill" style={{ width: `${percent}%` }} />
                  </View>
                  <Text className="stack-card-stat">
                    已答 {answered} · 正确 {correct} · {percent}%
                  </Text>
                </View>
              )
            })}
          </View>
        </View>
      </View>
    )
  }

  // ============ 渲染：答题中 =============
  if (mode === 'quiz' && currentSession && currentQ) {
    const progress = ((currentSession.currentIndex + 1) / currentSession.questionIds.length) * 100

    return (
      <View className="quiz-page quiz-mode">
        {/* 顶部进度 */}
        <View className="quiz-progress">
          <View className="progress-info">
            <Text className="progress-text">
              {currentSession.currentIndex + 1} / {currentSession.questionIds.length}
            </Text>
            <Button className="exit-btn" onClick={() => { endSession(); setMode('plan') }}>
              ✕ 退出
            </Button>
          </View>
          <View className="progress-bar">
            <View className="progress-fill" style={{ width: `${progress}%` }} />
          </View>
        </View>

        {/* 题目卡片 */}
        <View className="question-card">
          <View className="question-meta">
            <Text className="badge difficulty">{currentQ.difficulty}</Text>
            <Text className="badge category">{currentQ.category}</Text>
            <Text className="badge domain">{currentQ.domain}</Text>
          </View>

          <Text className="question-title">{currentQ.title}</Text>

          {currentQ.description && (
            <View className="question-desc">
              <Text className="desc-text">{currentQ.description}</Text>
            </View>
          )}

          {/* 选项（选择题） */}
          {currentQ.options && currentQ.options.length > 0 && !isAnswered && (
            <View className="options-list">
              {currentQ.options.map((opt, idx) => {
                const letter = opt.charAt(0)
                const isSelected = userAnswer === letter
                return (
                  <View
                    key={idx}
                    className={`option ${isSelected ? 'selected' : ''}`}
                    onClick={() => setUserAnswer(letter)}
                  >
                    <Text className="option-letter">{letter}</Text>
                    <Text className="option-text">{opt.substring(2)}</Text>
                  </View>
                )
              })}
            </View>
          )}

          {/* 答题区（非选择题） */}
          {(!currentQ.options || currentQ.options.length === 0) && !isAnswered && (
            <Textarea
              className="answer-input"
              placeholder="输入你的答案..."
              value={userAnswer}
              onInput={(e: any) => setUserAnswer(e.detail.value)}
            />
          )}

          {/* 已答反馈 */}
          {isAnswered && (
            <View className={`result-feedback ${isCorrect ? 'correct' : 'wrong'}`}>
              <View className="result-header">
                <Text className="result-icon">{isCorrect ? '✓' : '✗'}</Text>
                <Text className="result-text">{isCorrect ? '回答正确！' : '回答错误'}</Text>
              </View>
              {currentQ.options && (
                <Text className="correct-answer">正确答案：{currentQ.answer}</Text>
              )}
              {currentQ.explanation && (
                <View className="explanation">
                  <Text className="exp-label">💡 解析</Text>
                  <Text className="exp-text">{currentQ.explanation}</Text>
                </View>
              )}
            </View>
          )}

          {/* 操作按钮 */}
          <View className="quiz-actions">
            {!isAnswered ? (
              <Button
                className="btn-primary"
                onClick={handleSubmit}
                disabled={!userAnswer.trim()}
              >
                提交答案
              </Button>
            ) : (
              <>
                <Button
                  className="btn-secondary"
                  onClick={prevQuestion}
                  disabled={currentSession.currentIndex === 0}
                >
                  ← 上一题
                </Button>
                <Button className="btn-primary" onClick={handleNext}>
                  {currentSession.currentIndex === currentSession.questionIds.length - 1 ? '完成 ✓' : '下一题 →'}
                </Button>
              </>
            )}
          </View>
        </View>
      </View>
    )
  }

  // ============ 渲染：错题本 =============
  if (mode === 'wrong') {
    return (
      <View className="quiz-page wrong-mode">
        <View className="page-header">
          <Text className="page-title">❌ 错题本</Text>
          <Text className="page-sub">共 {wrongAnswers.length} 道错题</Text>
        </View>

        {wrongAnswers.length > 0 && (
          <Button className="btn-primary review-all-btn" onClick={startWrongQuiz}>
            🔁 重做错题（前 10 道）
          </Button>
        )}

        <View className="wrong-list">
          {wrongAnswers.map((wa, idx) => {
            const q = getQuestionById(wa.questionId)
            if (!q) return null
            return (
              <View key={wa.questionId} className="wrong-card">
                <View className="wrong-header">
                  <Text className="wrong-index">#{idx + 1}</Text>
                  <View className="badges">
                    <Text className="badge difficulty">{q.difficulty}</Text>
                    <Text className="badge category">{q.category}</Text>
                  </View>
                </View>
                <Text className="wrong-question">{q.title}</Text>
                <View className="wrong-answers">
                  <View className="answer-row">
                    <Text className="answer-label">你的回答:</Text>
                    <Text className="answer-text wrong">{wa.userAnswer || '(空)'}</Text>
                  </View>
                  <View className="answer-row">
                    <Text className="answer-label">正确答案:</Text>
                    <Text className="answer-text correct">
                      {String(q.answer).substring(0, 80)}{String(q.answer).length > 80 ? '...' : ''}
                    </Text>
                  </View>
                </View>
                <Text className="wrong-date">错答于 {wa.date}</Text>
              </View>
            )
          })}
        </View>

        {wrongAnswers.length === 0 && (
          <View className="empty-state">
            <Text className="empty-icon">🎊</Text>
            <Text className="empty-text">太棒了！暂无错题</Text>
          </View>
        )}

        <Button className="btn-secondary back-btn" onClick={() => setMode('plan')}>
          ← 返回计划
        </Button>
      </View>
    )
  }

  // ============ 渲染：题库列表 =============
  if (mode === 'list') {
    // 列表顶部统计：当前 stack 的题数 / 已答 / 答对
    const stackTotal = filter.stack === 'all' ? questions.length : ((quizStats as any)[filter.stack] || 0)
    const stackAnswered = filter.stack === 'all'
      ? stats.totalAnswered
      : (stackProgress[filter.stack]?.answered || 0)
    const stackCorrect = filter.stack === 'all'
      ? stats.totalCorrect
      : (stackProgress[filter.stack]?.correct || 0)

    return (
      <View className="quiz-page list-mode">
        <View className="page-header">
          <Text className="page-title">📚 题库浏览</Text>
          <Text className="page-sub">
            {filter.stack === 'all'
              ? `共 ${filteredQuestions.length} 题（总 ${questions.length}）`
              : `${filter.stack.toUpperCase()} 栈 · ${filteredQuestions.length} / ${(quizStats as any)[filter.stack] || 0} 题`}
          </Text>
        </View>

        {/* 顶部统计：当前 stack 题数 / 已答 / 答对 */}
        <View className="list-stats">
          <View className="list-stat">
            <Text className="stat-num">{stackTotal}</Text>
            <Text className="stat-label">{filter.stack === 'all' ? '总题数' : filter.stack.toUpperCase() + ' 题数'}</Text>
          </View>
          <View className="list-stat">
            <Text className="stat-num">{stackAnswered}</Text>
            <Text className="stat-label">已答</Text>
          </View>
          <View className="list-stat">
            <Text className="stat-num">{stackCorrect}</Text>
            <Text className="stat-label">答对</Text>
          </View>
        </View>

        {/* Stack 筛选 */}
        <View className="filter-bar">
          <View className="filter-group">
            <Text className="filter-label">技术栈</Text>
            <View className="filter-options">
              <View
                className={`chip ${filter.stack === 'all' ? 'active' : ''}`}
                onClick={() => setFilter({ ...filter, stack: 'all' })}
              >
                <Text>全部 ({questions.length})</Text>
              </View>
              {STACKS.map(s => (
                <View
                  key={s}
                  className={`chip stack-chip-${s} ${filter.stack === s ? 'active' : ''}`}
                  onClick={() => setFilter({ ...filter, stack: s })}
                >
                  <Text>{s.toUpperCase()} ({(quizStats as any)[s] || 0})</Text>
                </View>
              ))}
            </View>
          </View>
          <View className="filter-group">
            <Text className="filter-label">分类</Text>
            <View className="filter-options">
              {categories.map(cat => (
                <View
                  key={cat}
                  className={`chip ${filter.category === cat ? 'active' : ''}`}
                  onClick={() => setFilter({ ...filter, category: cat })}
                >
                  <Text>{cat}</Text>
                </View>
              ))}
            </View>
          </View>
          <View className="filter-group">
            <Text className="filter-label">难度</Text>
            <View className="filter-options">
              {difficulties.map(d => (
                <View
                  key={d}
                  className={`chip ${filter.difficulty === d ? 'active' : ''}`}
                  onClick={() => setFilter({ ...filter, difficulty: d })}
                >
                  <Text>{d}</Text>
                </View>
              ))}
            </View>
          </View>
        </View>

        {filteredQuestions.length > 0 && (
          <Button className="btn-primary start-btn" onClick={startListQuiz}>
            ▶ 开始前 20 题
          </Button>
        )}

        {/* 折叠式展开卡片 */}
        <View className="question-list">
          {filteredQuestions.slice(0, 50).map((q, idx) => {
            const isExpanded = expandedQuestions.has(q.id)
            const answeredTimes = answers.filter(a => a.questionId === q.id).length
            return (
              <View
                key={q.id}
                className={`question-list-item ${isExpanded ? 'question-list-item--expanded' : ''}`}
                data-question-id={q.id}
              >
                <View
                  className="item-summary"
                  onClick={() => toggleExpand(q.id)}
                >
                  <View className="item-index">{idx + 1}</View>
                  <View className="item-body">
                    <Text className="item-title">{q.title}</Text>
                    <View className="item-meta">
                      <Text className="badge difficulty">{q.difficulty}</Text>
                      <Text className="badge category">{q.category}</Text>
                      <Text className="badge domain">{q.domain}</Text>
                      {answeredTimes > 0 && (
                        <Text className="badge answered">已答 {answeredTimes} 次</Text>
                      )}
                    </View>
                  </View>
                  <Text className="expand-icon">{isExpanded ? '▼' : '▶'}</Text>
                </View>

                {isExpanded && (
                  <View className="item-detail">
                    {q.description && (
                      <Text className="item-desc">
                        {q.description.length > 100 ? q.description.substring(0, 100) + '...' : q.description}
                      </Text>
                    )}
                    {q.options && q.options.length > 0 && (
                      <View className="item-options">
                        {q.options.slice(0, 4).map((opt, oi) => (
                          <Text key={oi} className="item-option">• {opt}</Text>
                        ))}
                      </View>
                    )}
                    <View className="item-actions">
                      <Button
                        className="btn-primary"
                        onClick={() => {
                          startSession([q.id])
                          setSessionStartTime(Date.now())
                          setUserAnswer('')
                          setMode('quiz')
                        }}
                      >
                        ▶ 直接答此题
                      </Button>
                    </View>
                  </View>
                )}
              </View>
            )
          })}
        </View>

        {filteredQuestions.length === 0 && (
          <View className="empty-state">
            <Text className="empty-icon">📭</Text>
            <Text className="empty-text">没有符合条件的题目</Text>
          </View>
        )}

        {filteredQuestions.length > 50 && (
          <Text className="more-hint">仅显示前 50 道，共 {filteredQuestions.length} 道</Text>
        )}

        <Button className="btn-secondary back-btn" onClick={() => setMode('plan')}>
          ← 返回计划
        </Button>
      </View>
    )
  }

  // ============ 渲染：学习统计 =============
  if (mode === 'stats') {
    const allProgress = getProgress()
    return (
      <View className="quiz-page stats-mode">
        <View className="page-header">
          <Text className="page-title">📊 学习统计</Text>
          <Text className="page-sub">详细数据</Text>
        </View>

        <View className="stats-cards">
          <View className="stats-card">
            <Text className="stats-num">{stats.totalAnswered}</Text>
            <Text className="stats-label">累计答题</Text>
          </View>
          <View className="stats-card">
            <Text className="stats-num">{stats.totalCorrect}</Text>
            <Text className="stats-label">答对</Text>
          </View>
          <View className="stats-card">
            <Text className="stats-num">{Math.round(stats.accuracy * 100)}%</Text>
            <Text className="stats-label">正确率</Text>
          </View>
          <View className="stats-card">
            <Text className="stats-num">{stats.streak}</Text>
            <Text className="stats-label">连续天数</Text>
          </View>
        </View>

        <View className="progress-section">
          <Text className="section-title">整体进度</Text>
          <View className="overall-progress">
            <Text className="overall-text">
              {allProgress.answered} / {allProgress.total}
            </Text>
            <View className="progress-bar">
              <View
                className="progress-fill"
                style={{ width: `${(allProgress.answered / allProgress.total) * 100}%` }}
              />
            </View>
            <Text className="overall-percent">
              {((allProgress.answered / allProgress.total) * 100).toFixed(2)}%
            </Text>
          </View>
        </View>

        <View className="stack-progress-section">
          <Text className="section-title">各栈详细</Text>
          {Object.entries(stackProgress).length === 0 ? (
            <Text className="empty-text">暂无数据</Text>
          ) : (
            Object.entries(stackProgress).map(([stack, p]) => (
              <View key={stack} className="stack-detail">
                <View className="stack-detail-header">
                  <Text className="stack-name-large">{stack.toUpperCase()}</Text>
                  <Text className="stack-last">{p.lastUpdated}</Text>
                </View>
                <View className="stack-progress-bar">
                  <View className="progress-bar">
                    <View className="progress-fill" style={{ width: `${p.total > 0 ? (p.answered / p.total) * 100 : 0}%` }} />
                  </View>
                </View>
                <View className="stack-stats-row">
                  <Text className="stack-stat-item">已答: {p.answered}</Text>
                  <Text className="stack-stat-item">正确: {p.correct}</Text>
                  <Text className="stack-stat-item">准确率: {p.answered > 0 ? Math.round((p.correct / p.answered) * 100) : 0}%</Text>
                </View>
              </View>
            ))
          )}
        </View>

        <Button className="btn-secondary back-btn" onClick={() => setMode('plan')}>
          ← 返回计划
        </Button>
      </View>
    )
  }

  // fallback
  return <View className="quiz-page"><Text>Loading...</Text></View>
}

export default Quiz