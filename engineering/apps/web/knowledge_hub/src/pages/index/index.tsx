/**
 * 仪表盘页面
 *
 * 完整复刻自旧版 dashboard.html：
 * - 五年计划摘要卡
 * - 总览 5 卡（知识点总数 / 已测 / 平均分 / 已掌握 / 累计测验）
 * - 7 技术栈进度卡
 * - 高估盲点（自评"掌握"但测分 < 65）
 * - 最近测验 10 条
 * - 热力图（最近 53 周）
 * - 趋势折线图（最近 30 次）
 * - 五年成长路线 5 阶段
 */
import { useMemo, useState } from 'react'
import { View, Text } from '@tarojs/components'
import { useQuizStore, QuizAnswer } from '@/stores/quizStore'
import { useFiveYearPlanStore } from '@/stores/fiveYearPlanStore'
import { useInterviewBankStore } from '@/stores/interviewBankStore'
import { useDailyDataStore } from '@/stores/dailyDataStore'
import { quizByStack, quizStats } from '@/data/quiz-index'
import { algoBank } from '@/data/interview_bank'
import { YEAR_CONFIG, ALL_YEARS, getAllItems } from '@/data/five_year_plan_config'
import { HeatmapCalendar, TrendChart } from '@/components/Charts'
import './index.scss'

// 7 个技术栈元数据
const STACK_META: Record<string, { label: string; icon: string; cls: string; color: string }> = {
  c:     { label: 'C 技术栈',     icon: '🔧', cls: 'c',     color: '#27ae60' },
  cpp:   { label: 'C++ 技术栈',   icon: '⚙️', cls: 'cpp',   color: '#2980b9' },
  ds:    { label: '数据结构',     icon: '🌳', cls: 'ds',    color: '#e67e22' },
  db:    { label: '数据库内核',   icon: '🗄️', cls: 'db',    color: '#8e44ad' },
  py:    { label: 'Python',       icon: '🐍', cls: 'py',    color: '#e74c3c' },
  linux: { label: 'Linux 技术栈', icon: '🐧', cls: 'linux', color: '#16a085' },
  vdb:   { label: '向量数据库',   icon: '🔮', cls: 'vdb',   color: '#f39c12' }
}

// 五年成长路线
const ROADMAP_STAGES = [
  {
    years: '2026', title: '筑基年', theme: '打好基础',
    points: [
      '补齐 C/C++/DS/DB/Python/Linux 主干知识点',
      '系统学习 Linux 系统编程与内核观测',
      '建立作息、运动、深度工作节律'
    ]
  },
  {
    years: '2027', title: '能力年', theme: '提升硬实力',
    points: [
      '把知识点升级为可讲清楚的项目能力',
      '强化系统设计、数据库与工程实践',
      '形成可复用的调试、测试和交付流程'
    ]
  },
  {
    years: '2028', title: '提质年', theme: '优化生活与认知',
    points: [
      '把学习节奏和生活质量一起拉稳',
      '强化输出、复盘和长期认知积累',
      '减少无效消耗，提升执行稳定性'
    ]
  },
  {
    years: '2029', title: '抗风险年', theme: '建立保障与自由',
    points: [
      '补齐职业、财务和健康三类风险防线',
      '让技能结构具备更强迁移与变现能力',
      '沉淀一套可长期复用的系统方法'
    ]
  },
  {
    years: '2030', title: '丰收年', theme: '全面进入良性循环',
    points: [
      '技术、项目、收入和生活进入正反馈',
      '保持稳定学习同时扩大成果输出',
      '把阶段经验固化为长期成长资产'
    ]
  }
]

// 难度对应颜色
const SCORE_COLORS = {
  high: '#10b981',
  mid: '#f59e0b',
  low: '#ef4444'
}

const Dashboard = () => {
  const answers = useQuizStore(s => s.answers)
  const stackProgress = useQuizStore(s => s.stackProgress)
  const fyp = useFiveYearPlanStore()
  const ibViewed = useInterviewBankStore(s => s.viewed)
  const dailyLogs = useDailyDataStore(s => s.dailyLogs)

  const today = new Date()
  const currentYear = today.getFullYear()
  const yearCfg = YEAR_CONFIG[currentYear] || YEAR_CONFIG[2026]

  // ============ 五年计划摘要 ============
  const fypTodayItems = getAllItems(currentYear)
  const fypTodayData = fyp.getDay(today.getFullYear(), today.getMonth() + 1, today.getDate())
  const fypDoneCount = fypTodayItems.filter(it => fypTodayData.checks[it.id]).length

  // ============ 7 技术栈统计 ============
  const stackStats = useMemo(() => {
    const out: Record<string, { total: number; tested: number; attempts: number; scoreSum: number; avgScore: number }> = {}
    for (const [catId, meta] of Object.entries(STACK_META)) {
      const items = quizByStack[catId] || []
      const total = items.length
      const testedIds = new Set<string>()
      let attempts = 0
      let scoreSum = 0
      for (const a of answers) {
        const item = items.find(it => it.id === a.questionId)
        if (item) {
          testedIds.add(a.questionId)
          attempts++
          scoreSum += a.correct ? 100 : 0
        }
      }
      out[catId] = {
        total,
        tested: testedIds.size,
        attempts,
        scoreSum,
        avgScore: attempts ? Math.round(scoreSum / attempts) : 0
      }
    }
    return out
  }, [answers])

  // 合并 stackProgress 里的 done/doing/review
  const stackKanban = useMemo(() => {
    const out: Record<string, { done: number; doing: number; review: number; todo: number }> = {}
    for (const catId of Object.keys(STACK_META)) {
      const total = (quizByStack[catId] || []).length
      const sp = stackProgress[catId]
      const done = sp?.correct || 0
      const answered = sp?.answered || 0
      const doing = Math.max(0, answered - done)
      const review = 0
      out[catId] = { done, doing, review, todo: Math.max(0, total - answered) }
    }
    return out
  }, [stackProgress])

  // ============ 总览 5 卡 ============
  const totals = useMemo(() => {
    let totalItems = 0, totalTested = 0, totalAttempts = 0, totalScore = 0, totalDone = 0
    for (const catId of Object.keys(STACK_META)) {
      const s = stackStats[catId]
      if (!s) continue
      totalItems += s.total
      totalTested += s.tested
      totalAttempts += s.attempts
      totalScore += s.scoreSum
      totalDone += stackKanban[catId]?.done || 0
    }
    return {
      totalItems,
      totalTested,
      totalAttempts,
      avgScore: totalAttempts ? Math.round(totalScore / totalAttempts) : 0,
      totalDone,
      testPct: totalItems ? Math.round(totalTested / totalItems * 100) : 0,
      masterPct: totalItems ? Math.round(totalDone / totalItems * 100) : 0
    }
  }, [stackStats, stackKanban])

  // ============ 盲点：未掌握 + 测分 < 65 ============
  const blindSpots = useMemo(() => {
    const out: Array<{ catId: string; title: string; score: number; meta: typeof STACK_META[string] }> = []
    for (const [catId, meta] of Object.entries(STACK_META)) {
      const items = quizByStack[catId] || []
      for (const item of items) {
        const recent = answers.filter(a => a.questionId === item.id)
        if (recent.length === 0) continue
        const last3 = recent.slice(-3)
        const wrongRate = last3.filter(a => !a.correct).length / last3.length
        const score = Math.round((1 - wrongRate) * 100)
        if (score < 65) {
          out.push({ catId, title: item.title, score, meta })
        }
      }
    }
    return out.sort((a, b) => a.score - b.score).slice(0, 8)
  }, [answers])

  // ============ 最近测验 10 条 ============
  const recentRecords = useMemo(() => {
    return answers
      .filter(a => a.timestamp)
      .sort((a, b) => b.timestamp - a.timestamp)
      .slice(0, 10)
      .map(a => {
        const catId = Object.keys(STACK_META).find(cid =>
          (quizByStack[cid] || []).some(q => q.id === a.questionId)
        ) || 'unknown'
        const meta = STACK_META[catId]
        const item = (quizByStack[catId] || []).find(q => q.id === a.questionId)
        return { ...a, catId, meta, itemTitle: item?.title || a.questionId }
      })
  }, [answers])

  // 渲染：技术栈卡
  const renderStackCard = (catId: string) => {
    const meta = STACK_META[catId]
    const stats = stackStats[catId]
    const kb = stackKanban[catId]
    if (!stats) return null
    const masterPct = Math.round(kb.done / stats.total * 100)
    const testPct = Math.round(stats.tested / stats.total * 100)
    return (
      <View
        key={catId}
        className={`stack-card ${meta.cls}`}
        style={{ '--accent': meta.color } as any}
        onClick={() => window.location.assign('/quiz')}
      >
        <View className="stack-header">
          <Text className="stack-icon">{meta.icon}</Text>
          <View className="stack-info">
            <Text className="stack-name">{meta.label}</Text>
            <Text className="stack-sub">{stats.total} 知识点</Text>
          </View>
        </View>
        <View className="bar-wrap">
          <View
            className="bar-fill"
            style={{ width: `${masterPct}%`, background: meta.color }}
          />
        </View>
        <View className="stack-meta">
          <Text>已掌握 {kb.done}/{stats.total} ({masterPct}%)</Text>
          <Text>测评覆盖 {testPct}%</Text>
        </View>
        <View className="stack-meta stack-kanban">
          <Text style={{ color: '#10b981' }}>✅{kb.done}</Text>
          <Text style={{ color: '#3b82f6' }}>📖{kb.doing}</Text>
          <Text style={{ color: '#f59e0b' }}>🔄{kb.review}</Text>
          <Text style={{ color: '#94a3b8' }}>📥{kb.todo}</Text>
          <Text style={{ color: meta.color }}>🎯均分{stats.avgScore || '--'}</Text>
        </View>
      </View>
    )
  }

  // 跳转（用 popstate 触发 React Router）
  const navigate = (path: string) => {
    window.history.pushState(null, '', path)
    window.dispatchEvent(new PopStateEvent('popstate'))
  }

  return (
    <View className="dashboard">
      {/* 头部 */}
      <View className="dashboard-header">
        <Text className="greeting">📊 学习全景仪表盘</Text>
        <Text className="subtitle">{currentYear} 年 · {yearCfg.name} · {yearCfg.core}</Text>
      </View>

      {/* 五年计划摘要卡 */}
      <View className="plan-card">
        <Text className="plan-label">🏗️ 五年建设计划 · 当前进程</Text>
        <View className="plan-row">
          <View className="plan-info">
            <Text className="plan-year">{currentYear} — {yearCfg.name}</Text>
            <Text className="plan-theme">核心目标：{yearCfg.core}</Text>
            <Text className="plan-progress">今日打卡：<Text className="plan-strong">{fypDoneCount}/{fypTodayItems.length}</Text> 项完成</Text>
          </View>
        </View>
        <View className="plan-chips">
          {fypTodayItems.slice(0, 8).map(item => (
            <Text
              key={item.id}
              className={`plan-chip ${fypTodayData.checks[item.id] ? 'done' : ''}`}
            >
              {fypTodayData.checks[item.id] ? '✅' : '⬜'} {item.label}
            </Text>
          ))}
        </View>
        <View
          className="plan-link"
          onClick={() => navigate('/five_year_plan')}
        >
          <Text>查看详细计划 →</Text>
        </View>
      </View>

      {/* 总览 5 卡 */}
      <View className="overview-grid">
        <View className="stat-card">
          <Text className="num">{totals.totalItems}</Text>
          <Text className="lbl">知识点总数</Text>
        </View>
        <View className="stat-card">
          <Text className="num" style={{ color: '#2980b9' }}>{totals.totalTested}</Text>
          <Text className="lbl">已测知识点</Text>
          <Text className="trend">{totals.testPct}% 覆盖率</Text>
        </View>
        <View className="stat-card">
          <Text
            className="num"
            style={{ color: totals.avgScore >= 80 ? SCORE_COLORS.high : totals.avgScore >= 60 ? SCORE_COLORS.mid : SCORE_COLORS.low }}
          >
            {totals.avgScore || '--'}
          </Text>
          <Text className="lbl">平均测验分</Text>
        </View>
        <View className="stat-card">
          <Text className="num" style={{ color: '#27ae60' }}>{totals.totalDone}</Text>
          <Text className="lbl">已掌握</Text>
          <Text className="trend">{totals.masterPct}% 掌握率</Text>
        </View>
        <View className="stat-card">
          <Text className="num" style={{ color: '#e67e22' }}>{totals.totalAttempts}</Text>
          <Text className="lbl">累计测验次数</Text>
          <Text className="trend">{Object.keys(STACK_META).length} 个技术栈</Text>
        </View>
      </View>

      {/* 7 技术栈进度卡 */}
      <Text className="section-title">⚙️ 各技术栈进度</Text>
      <View className="stacks-grid">
        {Object.keys(STACK_META).map(renderStackCard)}
      </View>

      {/* 双栏：盲点 + 最近测验 */}
      <View className="two-col">
        <View className="section">
          <View className="section-title">
            <Text>🔴 高估盲点 </Text>
            <Text className="section-subtitle">（自评掌握但测分 {'<'} 65）</Text>
          </View>
          {blindSpots.length === 0 ? (
            <View className="empty">🎉 暂无明显盲点，继续保持！</View>
          ) : (
            <View className="blind-list">
              {blindSpots.map((b, i) => (
                <View
                  key={i}
                  className="blind-item"
                  onClick={() => navigate('/quiz')}
                >
                  <Text className="blind-badge">{b.meta?.icon} {b.score}分</Text>
                  <Text className="blind-name">{b.title}</Text>
                  <Text className="blind-arrow">去测试 →</Text>
                </View>
              ))}
            </View>
          )}
        </View>

        <View className="section">
          <View className="section-title">
            <Text>⏱️ 最近测验记录</Text>
          </View>
          {recentRecords.length === 0 ? (
            <View className="empty">尚无测验记录，去 <Text className="link" onClick={() => navigate('/quiz')}>评测系统</Text> 开始第一次测试！</View>
          ) : (
            <View className="recent-list">
              {recentRecords.map(r => (
                <View key={r.questionId + r.timestamp} className="recent-item">
                  <Text
                    className="recent-score"
                    style={{ color: r.correct ? SCORE_COLORS.high : SCORE_COLORS.low }}
                  >
                    {r.correct ? '✓' : '✗'}
                  </Text>
                  <View className="recent-info">
                    <Text className="recent-name">{r.meta?.icon} {r.itemTitle}</Text>
                    <Text className="recent-meta">{r.meta?.label || r.catId}</Text>
                  </View>
                  <Text className="recent-time">
                    {new Date(r.timestamp).getMonth() + 1}/{new Date(r.timestamp).getDate()}
                  </Text>
                </View>
              ))}
            </View>
          )}
        </View>
      </View>

      {/* 双栏：热力图 + 趋势图 */}
      <View className="two-col">
        <View className="section">
          <View className="section-title">
            <Text>🔥 学习热力图 </Text>
            <Text className="section-subtitle">（每次测验计为1次活动）</Text>
          </View>
          <HeatmapCalendar weeks={26} cellSize={10} gap={2} />
        </View>
        <View className="section">
          <View className="section-title">
            <Text>📈 最近 30 次测验得分趋势</Text>
          </View>
          <TrendChart days={30} height={180} />
        </View>
      </View>

      {/* 五年成长路线 */}
      <View className="section roadmap-section">
        <View className="section-title">
          <Text>🛤️ 五年成长路线 </Text>
          <Text className="section-subtitle">至少三阶段连续推进</Text>
        </View>
        <View className="roadmap-grid">
          {ROADMAP_STAGES.map(stage => {
            const isActive = String(currentYear) === stage.years
            return (
              <View
                key={stage.years}
                className={`roadmap-stage ${isActive ? 'active' : ''}`}
                onClick={() => navigate('/five_year_plan')}
              >
                <Text className="stage-years">{stage.years}</Text>
                <Text className="stage-title">{stage.title}</Text>
                <Text className="stage-theme">{stage.theme}</Text>
                <View className="stage-points">
                  {stage.points.map((p, i) => (
                    <Text key={i} className="stage-point">{p}</Text>
                  ))}
                </View>
              </View>
            )
          })}
        </View>
        <View className="roadmap-link-bar">
          <View
            className="roadmap-link"
            onClick={() => navigate('/five_year_plan')}
          >
            <Text>查看完整计划 →</Text>
          </View>
        </View>
      </View>

      <View className="safe-area-bottom" />
    </View>
  )
}

export default Dashboard
