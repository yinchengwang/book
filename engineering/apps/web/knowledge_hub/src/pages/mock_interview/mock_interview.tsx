import { Component, useState, useEffect } from 'react'
import { View, Text, Button, Input, Textarea } from '@tarojs/components'
import { useMockInterviewStore } from '@/stores/mockInterviewStore'
import { useSettingsStore } from '@/stores/settingsStore'
import './mock_interview.scss'

// AI 服务接口（预埋）
interface AIService {
  generateFollowUp: (question: string, answer: string, context?: any) => Promise<string[]>
  evaluateAnswer: (question: string, answer: string) => Promise<{
    score: number
    feedback: string
    suggestions: string[]
  }>
  generateNewQuestion: (topic: string, difficulty: string) => Promise<any>
}

// Mock AI 服务实现（实际接入时请替换为真实 API）
const mockAIService: AIService = {
  generateFollowUp: async (question, answer, context) => {
    // 模拟 AI 生成追问
    await new Promise(resolve => setTimeout(resolve, 800))

    // 基于问题和回答生成相关追问
    const followUps: Record<string, string[]> = {
      'LRU': [
        '为什么选择双向链表而不是其他数据结构？',
        '如果要求 O(1) 获取最近使用的 N 个元素，如何优化？',
        '如何处理并发访问的情况？'
      ],
      'default': [
        '能详细解释一下你选择这种方法的原因吗？',
        '时间复杂度和空间复杂度分别是多少？',
        '有没有其他的实现方式？各自的优缺点是什么？'
      ]
    }

    // 根据关键词匹配
    const key = Object.keys(followUps).find(k =>
      question.toLowerCase().includes(k.toLowerCase())
    ) || 'default'

    return followUps[key]
  },

  evaluateAnswer: async (question, answer) => {
    // 模拟 AI 评估
    await new Promise(resolve => setTimeout(resolve, 1200))

    // 简单的规则评估（实际应使用 NLP/AI 模型）
    const wordCount = answer.split(/[\s,，。.]+/).filter(w => w.length > 0).length
    const hasStructure = answer.includes('\n') || answer.includes('1.') || answer.includes('•')
    const hasCode = answer.includes('```') || answer.includes('class ') || answer.includes('function ')

    let score = 60
    let feedback = '回答基本完整，但可以进一步优化。'
    const suggestions: string[] = []

    if (wordCount < 20) {
      score -= 20
      suggestions.push('回答过于简短，建议补充更多细节')
    }

    if (!hasStructure) {
      score -= 10
      suggestions.push('建议使用结构化方式组织回答（如分点）')
    }

    if (score >= 90) {
      feedback = '回答非常出色！结构清晰，内容完整。'
    } else if (score >= 75) {
      feedback = '回答不错，但还有一些细节可以补充。'
    } else if (score >= 60) {
      feedback = '回答基本到位，建议加深对原理的理解。'
    } else {
      feedback = '回答不够完整，建议重新复习相关知识点。'
    }

    return { score, feedback, suggestions }
  },

  generateNewQuestion: async (topic, difficulty) => {
    await new Promise(resolve => setTimeout(resolve, 600))

    // Mock 生成新题目
    return {
      id: `ai-${Date.now()}`,
      title: `【AI 生成】关于 ${topic} 的进阶问题`,
      description: `AI 根据你的回答动态生成的追问题目`,
      category: '简答题',
      difficulty: difficulty,
      domain: topic,
      hint: '结合之前的回答思考这个问题',
      estimatedTime: 10
    }
  }
}

// 模拟面试页面
const MockInterview = () => {
  const { sessionHistory, addSession } = useMockInterviewStore()
  const { apiConfig } = useSettingsStore()
  const { apiKey, apiUrl: apiEndpoint } = apiConfig

  const [step, setStep] = useState<'config' | 'interview' | 'result'>('config')
  const [config, setConfig] = useState({
    role: '后端开发',
    difficulty: '中级',
    duration: 30,
    questions: 5,
    enableAI: true // AI 功能开关
  })

  const [currentQ, setCurrentQ] = useState(0)
  const [answers, setAnswers] = useState<Record<string, string>>({})
  const [timeLeft, setTimeLeft] = useState(0)
  const [timer, setTimer] = useState<NodeJS.Timeout | null>(null)

  // AI 相关状态
  const [followUps, setFollowUps] = useState<string[]>([])
  const [currentFollowUp, setCurrentFollowUp] = useState(0)
  const [evaluation, setEvaluation] = useState<{
    score: number
    feedback: string
    suggestions: string[]
  } | null>(null)
  const [isAIGenerating, setIsAIGenerating] = useState(false)
  const [isAIEvaluating, setIsAIEvaluating] = useState(false)

  // 示例题目库
  const questionBank = [
    {
      id: 1,
      category: '算法',
      question: '请实现一个 LRU 缓存',
      hint: '考虑使用哈希表 + 双向链表',
      keywords: ['LRU']
    },
    {
      id: 2,
      category: '系统设计',
      question: '设计一个短链接系统',
      hint: '考虑哈希碰撞、长链接存储',
      keywords: ['短链接', '系统设计']
    },
    {
      id: 3,
      category: 'C++',
      question: '解释一下 C++ 中的虚函数表',
      hint: '考虑多态实现原理',
      keywords: ['虚函数', '多态']
    },
    {
      id: 4,
      category: '数据库',
      question: 'MySQL 索引失效的场景有哪些？',
      hint: '考虑函数、类型转换、最左前缀',
      keywords: ['索引', 'MySQL']
    },
    {
      id: 5,
      category: '网络',
      question: 'TCP 四次挥手为什么需要 TIME_WAIT？',
      hint: '考虑可靠性和端口复用',
      keywords: ['TCP', '网络']
    }
  ]

  const currentQuestion = questionBank[currentQ]

  // 定时器
  useEffect(() => {
    if (step === 'interview' && timeLeft > 0) {
      const t = setInterval(() => {
        setTimeLeft(prev => {
          if (prev <= 1) {
            clearInterval(t)
            handleFinish()
            return 0
          }
          return prev - 1
        })
      }, 1000)
      setTimer(t)
      return () => clearInterval(t)
    }
  }, [step === 'interview'])

  const startInterview = () => {
    setStep('interview')
    setTimeLeft(config.duration * 60)
    setCurrentQ(0)
    setAnswers({})
    setFollowUps([])
    setEvaluation(null)
  }

  const handleFinish = () => {
    if (timer) clearInterval(timer)
    setStep('result')

    // 保存面试记录
    addSession({
      id: `session-${Date.now()}`,
      date: new Date().toISOString(),
      role: config.role,
      difficulty: config.difficulty,
      questions: questionBank.slice(0, config.questions).map((q, idx) => ({
        question: q.question,
        answer: answers[idx] || '',
        evaluation: evaluation || undefined
      })),
      totalScore: evaluation?.score || 0
    })
  }

  const handleNext = async () => {
    if (currentQ < config.questions - 1) {
      // AI 评估当前回答
      if (config.enableAI && answers[currentQ]) {
        setIsAIEvaluating(true)
        const evalResult = await mockAIService.evaluateAnswer(
          currentQuestion.question,
          answers[currentQ]
        )
        setEvaluation(evalResult)
        setIsAIEvaluating(false)

        // 生成追问
        setIsAIGenerating(true)
        const followUpQuestions = await mockAIService.generateFollowUp(
          currentQuestion.question,
          answers[currentQ]
        )
        setFollowUps(followUpQuestions)
        setCurrentFollowUp(0)
        setIsAIGenerating(false)
      }

      setCurrentQ(currentQ + 1)
    } else {
      handleFinish()
    }
  }

  const handlePrev = () => {
    if (currentQ > 0) {
      setCurrentQ(currentQ - 1)
    }
  }

  // 回答追问
  const answerFollowUp = async (followUpIdx: number) => {
    // 可以在这里添加追问的回答逻辑
    console.log(`回答追问 ${followUpIdx}:`, answers[`followup-${currentQ}-${followUpIdx}`])
  }

  // 跳过追问继续下一题
  const skipFollowUps = () => {
    setFollowUps([])
    handleNext()
  }

  // 显示/隐藏追问面板
  const [showFollowUps, setShowFollowUps] = useState(false)

  return (
    <View className="mock-page">
      <View className="mock-header">
        <Text className="page-title">模拟面试</Text>
        <View className="ai-badge">
          <Text className="ai-icon">🤖</Text>
          <Text className="ai-text">AI 辅助模式</Text>
        </View>
      </View>

      {/* 配置阶段 */}
      {step === 'config' && (
        <View className="config-section">
          <View className="config-card">
            <Text className="card-title">面试配置</Text>

            <View className="config-item">
              <Text className="config-label">应聘岗位</Text>
              <View className="role-options">
                {['后端开发', '前端开发', '全栈', '算法工程师'].map(role => (
                  <View
                    key={role}
                    className={`role-chip ${config.role === role ? 'active' : ''}`}
                    onClick={() => setConfig({ ...config, role })}
                  >
                    <Text>{role}</Text>
                  </View>
                ))}
              </View>
            </View>

            <View className="config-item">
              <Text className="config-label">难度级别</Text>
              <View className="role-options">
                {['入门', '初级', '中级', '高级', '专家'].map(diff => (
                  <View
                    key={diff}
                    className={`role-chip ${config.difficulty === diff ? 'active' : ''}`}
                    onClick={() => setConfig({ ...config, difficulty: diff })}
                  >
                    <Text>{diff}</Text>
                  </View>
                ))}
              </View>
            </View>

            <View className="config-item">
              <Text className="config-label">时长: {config.duration} 分钟</Text>
              <View className="slider-track">
                <View
                  className="slider-fill"
                  style={{ width: `${(config.duration / 60) * 100}%` }}
                />
              </View>
              <View className="slider-labels">
                <Text>5分钟</Text>
                <Text>60分钟</Text>
              </View>
            </View>

            <View className="config-item">
              <Text className="config-label">题目数量: {config.questions}</Text>
              <View className="role-options">
                {[3, 5, 8, 10].map(num => (
                  <View
                    key={num}
                    className={`role-chip ${config.questions === num ? 'active' : ''}`}
                    onClick={() => setConfig({ ...config, questions: num })}
                  >
                    <Text>{num}题</Text>
                  </View>
                ))}
              </View>
            </View>

            {/* AI 功能开关 */}
            <View className="config-item ai-toggle">
              <Text className="config-label">🤖 AI 辅助功能</Text>
              <View className="toggle-row">
                <Text className="toggle-desc">开启后可获得 AI 追问和智能评估</Text>
                <View
                  className={`toggle-switch ${config.enableAI ? 'active' : ''}`}
                  onClick={() => setConfig({ ...config, enableAI: !config.enableAI })}
                >
                  <View className="toggle-dot" />
                </View>
              </View>
            </View>
          </View>

          {/* 历史记录入口 */}
          {sessionHistory.length > 0 && (
            <View className="history-section">
              <Text className="history-title">📋 历史记录</Text>
              <View className="history-list">
                {sessionHistory.slice(-3).reverse().map(session => (
                  <View key={session.id} className="history-item">
                    <View className="history-info">
                      <Text className="history-role">{session.role}</Text>
                      <Text className="history-date">
                        {new Date(session.date).toLocaleDateString()}
                      </Text>
                    </View>
                    <View className="history-score">
                      <Text className="score-value">{session.totalScore}</Text>
                      <Text className="score-label">分</Text>
                    </View>
                  </View>
                ))}
              </View>
            </View>
          )}

          <Button className="start-btn" onClick={startInterview}>
            🚀 开始模拟面试
          </Button>
        </View>
      )}

      {/* 面试阶段 */}
      {step === 'interview' && currentQuestion && (
        <View className="interview-section">
          {/* 进度条 */}
          <View className="progress-bar">
            <View className="progress-info">
              <Text className="progress-text">
                题目 {currentQ + 1} / {config.questions}
              </Text>
              <Text className="time-text">
                ⏱️ {Math.floor(timeLeft / 60)}:{String(timeLeft % 60).padStart(2, '0')}
              </Text>
            </View>
            <View className="progress-track">
              <View
                className="progress-fill"
                style={{ width: `${((currentQ + 1) / config.questions) * 100}%` }}
              />
            </View>
          </View>

          {/* 题目卡片 */}
          <View className="question-card">
            <View className="question-header">
              <Text className="category-badge">{currentQuestion.category}</Text>
              {config.enableAI && (
                <Text className="ai-indicator">🤖 AI</Text>
              )}
            </View>
            <Text className="question-text">{currentQuestion.question}</Text>
            <View className="hint-box">
              <Text className="hint-label">💡 提示:</Text>
              <Text className="hint-text">{currentQuestion.hint}</Text>
            </View>
          </View>

          {/* 答题区 */}
          <View className="answer-section">
            <Text className="answer-label">你的回答:</Text>
            <Textarea
              className="answer-input"
              placeholder="输入你的回答..."
              value={answers[currentQ] || ''}
              onInput={(e: any) => setAnswers({ ...answers, [currentQ]: e.detail.value })}
              maxlength={2000}
            />

            {/* AI 评估结果 */}
            {isAIEvaluating && (
              <View className="ai-processing">
                <Text className="ai-icon spinning">🤖</Text>
                <Text className="ai-text">AI 正在评估你的回答...</Text>
              </View>
            )}

            {evaluation && !isAIEvaluating && (
              <View className="evaluation-result">
                <View className="eval-header">
                  <Text className="eval-title">📊 AI 评估</Text>
                  <View className="eval-score">
                    <Text className="score-num">{evaluation.score}</Text>
                    <Text className="score-unit">/ 100</Text>
                  </View>
                </View>
                <Text className="eval-feedback">{evaluation.feedback}</Text>
                {evaluation.suggestions.length > 0 && (
                  <View className="eval-suggestions">
                    <Text className="suggest-title">💡 改进建议:</Text>
                    {evaluation.suggestions.map((s, idx) => (
                      <Text key={idx} className="suggest-item">• {s}</Text>
                    ))}
                  </View>
                )}
              </View>
            )}
          </View>

          {/* AI 追问面板 */}
          {followUps.length > 0 && !isAIGenerating && (
            <View className="followup-panel">
              <View className="followup-header">
                <Text className="followup-title">🤖 AI 追问</Text>
                <Button className="skip-btn" onClick={skipFollowUps}>
                  跳过 →
                </Button>
              </View>
              <View className="followup-list">
                {followUps.map((followUp, idx) => (
                  <View key={idx} className="followup-item">
                    <Text className="followup-q">{idx + 1}. {followUp}</Text>
                    <Input
                      className="followup-input"
                      placeholder="你的回答..."
                      value={answers[`followup-${currentQ}-${idx}`] || ''}
                      onInput={(e: any) => setAnswers({
                        ...answers,
                        [`followup-${currentQ}-${idx}`]: e.detail.value
                      })}
                    />
                  </View>
                ))}
              </View>
            </View>
          )}

          {isAIGenerating && (
            <View className="ai-generating">
              <Text className="ai-icon spinning">🤖</Text>
              <Text className="ai-text">AI 正在生成追问...</Text>
            </View>
          )}

          {/* 导航按钮 */}
          <View className="nav-buttons">
            <Button
              className="nav-btn"
              disabled={currentQ === 0}
              onClick={handlePrev}
            >
              上一题
            </Button>
            <Button className="nav-btn primary" onClick={handleNext}>
              {currentQ === config.questions - 1 ? '提交' : '下一题'}
            </Button>
          </View>
        </View>
      )}

      {/* 结果阶段 */}
      {step === 'result' && (
        <View className="result-section">
          <View className="result-header">
            <Text className="result-icon">🎉</Text>
            <Text className="result-title">面试完成!</Text>
          </View>

          {/* 综合评估 */}
          {config.enableAI && evaluation && (
            <View className="overall-evaluation">
              <Text className="eval-section-title">📊 综合评估</Text>
              <View className="eval-card">
                <View className="eval-score-large">
                  <Text className="big-score">{evaluation.score}</Text>
                  <Text className="big-score-label">综合得分</Text>
                </View>
                <View className="eval-breakdown">
                  <View className="breakdown-item">
                    <Text className="breakdown-label">回答完整度</Text>
                    <View className="breakdown-bar">
                      <View className="breakdown-fill" style={{ width: '75%' }} />
                    </View>
                  </View>
                  <View className="breakdown-item">
                    <Text className="breakdown-label">逻辑清晰度</Text>
                    <View className="breakdown-bar">
                      <View className="breakdown-fill" style={{ width: '80%' }} />
                    </View>
                  </View>
                  <View className="breakdown-item">
                    <Text className="breakdown-label">技术深度</Text>
                    <View className="breakdown-bar">
                      <View className="breakdown-fill" style={{ width: '65%' }} />
                    </View>
                  </View>
                </View>
              </View>
            </View>
          )}

          <View className="result-summary">
            <View className="summary-item">
              <Text className="summary-value">{config.questions}</Text>
              <Text className="summary-label">完成题目</Text>
            </View>
            <View className="summary-item">
              <Text className="summary-value">{followUps.length}</Text>
              <Text className="summary-label">AI 追问</Text>
            </View>
            <View className="summary-item">
              <Text className="summary-value">
                {config.enableAI ? (evaluation?.score || '-') : '待评估'}
              </Text>
              <Text className="summary-label">得分</Text>
            </View>
          </View>

          {/* AI 功能总结 */}
          {config.enableAI && (
            <View className="ai-summary">
              <Text className="ai-summary-title">🤖 AI 功能总结</Text>
              <View className="ai-features">
                <View className="feature-item">
                  <Text className="feature-icon">✨</Text>
                  <Text className="feature-text">智能追问已生成 {followUps.length} 个</Text>
                </View>
                <View className="feature-item">
                  <Text className="feature-icon">📝</Text>
                  <Text className="feature-text">详细评估反馈已提供</Text>
                </View>
                <View className="feature-item">
                  <Text className="feature-icon">💡</Text>
                  <Text className="feature-text">改进建议已给出</Text>
                </View>
              </View>
            </View>
          )}

          <View className="result-actions">
            <Button
              className="result-btn secondary"
              onClick={() => {
                setStep('config')
                setEvaluation(null)
                setFollowUps([])
              }}
            >
              重新配置
            </Button>
            <Button className="result-btn primary">
              📤 导出面经
            </Button>
          </View>
        </View>
      )}
    </View>
  )
}

export default MockInterview
