/**
 * 复习计划页面
 *
 * 双 Tab：
 * - 待复习（dueReviews）：当前要复习的题（按时显示）
 * - 已计划（allRecords）：所有复习记录，包括未来的、已完成的（按 nextReviewDate 倒序）
 *
 * 即使全部复习完，"已计划" tab 仍可查看所有复习项
 */
import { useEffect, useState, useMemo } from 'react'
import { View, Text, Button } from '@tarojs/components'
import { useReviewStore } from '@/stores/reviewStore'
import './Review.scss'

type Tab = 'due' | 'all' | 'done'

const Review = () => {
  const {
    records,
    dueReviews,
    currentReview,
    loading,
    loadDueReviews,
    rateReview,
    nextReview
  } = useReviewStore()

  const [tab, setTab] = useState<Tab>('due')

  // 初始化加载
  useEffect(() => {
    loadDueReviews()
  }, [loadDueReviews])

  // 派生：所有记录按 nextReviewDate 倒序
  const sortedRecords = useMemo(() => {
    return [...records].sort((a, b) =>
      new Date(b.nextReviewDate).getTime() - new Date(a.nextReviewDate).getTime()
    )
  }, [records])

  // 派生：已完成（quality >= 3 且已过 nextReviewDate 的）
  const doneRecords = useMemo(() => {
    const now = new Date()
    return records.filter(r =>
      r.repetitions > 0 && new Date(r.nextReviewDate) > now
    ).sort((a, b) =>
      new Date(b.lastReviewDate).getTime() - new Date(a.lastReviewDate).getTime()
    )
  }, [records])

  // 派生：即将到期（7 天内）
  const upcomingRecords = useMemo(() => {
    const now = new Date().getTime()
    const week = 7 * 24 * 3600 * 1000
    return records.filter(r => {
      const t = new Date(r.nextReviewDate).getTime()
      return t > now && t - now < week
    })
  }, [records])

  // 派生：已延期（nextReviewDate 在过去但还没复习）
  const overdueRecords = useMemo(() => {
    const now = new Date().getTime()
    return records.filter(r => new Date(r.nextReviewDate).getTime() < now && r.repetitions === 0)
  }, [records])

  return (
    <View className="review-page">
      <View className="review-header">
        <Text className="review-title">📖 复习计划</Text>
        <Text className="review-subtitle">
          SM-2 间隔重复算法 · 基于艾宾浩斯遗忘曲线
        </Text>
      </View>

      {/* 顶部统计 - 4 卡可点击切换到对应 tab */}
      <View className="review-stats">
        <View
          className={`stat-card stat-due ${tab === 'due' ? 'active' : ''}`}
          onClick={() => setTab('due')}
        >
          <Text className="stat-num">{dueReviews.length}</Text>
          <Text className="stat-label">待复习</Text>
        </View>
        <View
          className={`stat-card stat-upcoming ${tab === 'all' ? 'active' : ''}`}
          onClick={() => setTab('all')}
        >
          <Text className="stat-num">{upcomingRecords.length}</Text>
          <Text className="stat-label">7天内</Text>
        </View>
        <View
          className={`stat-card stat-overdue ${tab === 'all' ? 'active' : ''}`}
          onClick={() => setTab('all')}
        >
          <Text className="stat-num">{overdueRecords.length}</Text>
          <Text className="stat-label">已延期</Text>
        </View>
        <View
          className={`stat-card stat-done ${tab === 'done' ? 'active' : ''}`}
          onClick={() => setTab('done')}
        >
          <Text className="stat-num">{doneRecords.length}</Text>
          <Text className="stat-label">已掌握</Text>
        </View>
      </View>

      {/* Tab 切换 */}
      <View className="review-tabs">
        <View
          className={`tab ${tab === 'due' ? 'active' : ''}`}
          onClick={() => setTab('due')}
        >
          <Text>🎯 待复习 ({dueReviews.length})</Text>
        </View>
        <View
          className={`tab ${tab === 'all' ? 'active' : ''}`}
          onClick={() => setTab('all')}
        >
          <Text>📋 全部计划 ({records.length})</Text>
        </View>
        <View
          className={`tab ${tab === 'done' ? 'active' : ''}`}
          onClick={() => setTab('done')}
        >
          <Text>✅ 已完成 ({doneRecords.length})</Text>
        </View>
      </View>

      {/* === 待复习 Tab === */}
      {tab === 'due' && (
        <>
          {loading ? (
            <View className="review-loading">加载中...</View>
          ) : dueReviews.length === 0 ? (
            <View className="review-empty">
              <Text className="empty-icon">🎉</Text>
              <Text className="empty-text">今日复习已完成！</Text>
              <Text className="empty-sub">
                {overdueRecords.length > 0
                  ? `还有 ${overdueRecords.length} 题已延期，去"全部计划"重置吧`
                  : '明天再来吧~'}
              </Text>
            </View>
          ) : currentReview ? (
            <View className="review-card">
              <View className="card-domain">
                {currentReview.domain}
              </View>
              <View className="card-content">
                <Text className="card-question">{currentReview.question}</Text>
              </View>
              <View className="card-answer">
                <Text className="answer-label">答案</Text>
                <Text className="answer-text">{currentReview.answer}</Text>
              </View>

              <View className="rating-section">
                <Text className="rating-prompt">这次复习你感觉如何？</Text>
                <View className="rating-buttons">
                  {[0, 1, 2, 3, 4, 5].map((q) => (
                    <Button
                      key={q}
                      className={`rating-btn rating-${q}`}
                      onClick={() => rateReview(q)}
                    >
                      {q === 0 && '忘记'}
                      {q === 1 && '难'}
                      {q === 2 && '较难'}
                      {q === 3 && '一般'}
                      {q === 4 && '较易'}
                      {q === 5 && '简单'}
                    </Button>
                  ))}
                </View>
              </View>
            </View>
          ) : (
            <View className="review-complete">
              <Text className="complete-icon">🎉</Text>
              <Text className="complete-text">本轮复习完成！</Text>
              <Button className="next-btn" onClick={nextReview}>
                继续下一轮
              </Button>
            </View>
          )}
        </>
      )}

      {/* === 全部计划 Tab === */}
      {tab === 'all' && (
        <View className="record-list">
          {sortedRecords.length === 0 ? (
            <View className="review-empty">
              <Text className="empty-icon">📋</Text>
              <Text className="empty-text">还没有复习计划</Text>
              <Text className="empty-sub">完成题目后会自动加入复习计划</Text>
            </View>
          ) : (
            sortedRecords.map(r => {
              const status = getRecordStatus(r, overdueRecords, doneRecords)
              return (
                <View key={r.id} className={`record-card record-${status.key}`}>
                  <View className="record-header">
                    <View className="record-domain">{r.domain}</View>
                    <View className={`record-status status-${status.key}`}>
                      <Text>{status.label}</Text>
                    </View>
                  </View>
                  <Text className="record-question">{r.question}</Text>
                  <View className="record-meta">
                    <Text className="meta-item">
                      📅 下次复习：{new Date(r.nextReviewDate).toLocaleDateString('zh-CN')}
                    </Text>
                    <Text className="meta-item">
                      🔁 重复 {r.repetitions} 次 · 易度 {r.easiness.toFixed(1)}
                    </Text>
                  </View>
                </View>
              )
            })
          )}
        </View>
      )}

      {/* === 已完成 Tab === */}
      {tab === 'done' && (
        <View className="record-list">
          {doneRecords.length === 0 ? (
            <View className="review-empty">
              <Text className="empty-icon">📚</Text>
              <Text className="empty-text">还没有已掌握的题目</Text>
              <Text className="empty-sub">连续答对几次后会自动归入这里</Text>
            </View>
          ) : (
            doneRecords.map(r => (
              <View key={r.id} className="record-card record-done">
                <View className="record-header">
                  <View className="record-domain">{r.domain}</View>
                  <View className="record-status status-done">
                    <Text>✅ 已掌握</Text>
                  </View>
                </View>
                <Text className="record-question">{r.question}</Text>
                <View className="record-meta">
                  <Text className="meta-item">
                    🕒 上次复习：{r.lastReviewDate
                      ? new Date(r.lastReviewDate).toLocaleDateString('zh-CN')
                      : '-'}
                  </Text>
                  <Text className="meta-item">
                    🔁 重复 {r.repetitions} 次
                  </Text>
                </View>
              </View>
            ))
          )}
        </View>
      )}
    </View>
  )
}

// 工具：判断单条记录的状态
function getRecordStatus(
  r: any,
  overdue: any[],
  done: any[]
): { key: 'overdue' | 'upcoming' | 'done'; label: string } {
  if (overdue.find(x => x.id === r.id)) {
    return { key: 'overdue', label: '⚠️ 已延期' }
  }
  if (done.find(x => x.id === r.id)) {
    return { key: 'done', label: '✅ 已掌握' }
  }
  return { key: 'upcoming', label: '🕒 计划中' }
}

export default Review