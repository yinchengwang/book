/**
 * 能力差距分析页面
 *
 * - 用户未学习时显示空状态引导（CTA 按钮跳到 /quiz 开始）
 * - 有数据时显示 5 领域差距 + 总体进度 + 学习建议
 */
import { useEffect, useMemo } from 'react'
import { View, Text, Progress } from '@tarojs/components'
import { useKnowledgeStore } from '@/stores/knowledgeStore'
import { useQuizStore } from '@/stores/quizStore'
import './gap_analysis.scss'

/**
 * 跨端跳转：H5 用 location，小程序用 Taro
 */
function navigateTo(path: string) {
  if (typeof window !== 'undefined') {
    // H5 端
    window.location.href = path
  } else {
    // 小程序端（Taro）
    try {
      // eslint-disable-next-line @typescript-eslint/no-require-imports
      const Taro = require('@tarojs/taro')
      Taro.navigateTo({ url: path })
    } catch {
      // ignore
    }
  }
}

const GapAnalysis = () => {
  const { gaps, loadGaps, nodes } = useKnowledgeStore()
  const answers = useQuizStore(s => s.answers)

  // 是否有真实学习数据
  const hasRealData = useMemo(() => {
    // 如果用户有答题记录，或 gaps 已被计算过
    if (answers && answers.length > 0) return true
    if (gaps && Object.keys(gaps).length > 0) {
      // 至少有一个领域的 level > 0
      return Object.values(gaps).some(g => g && g.level > 0)
    }
    return false
  }, [answers, gaps])

  // 页面加载时重新计算 gaps（基于实际答题数据）
  useEffect(() => {
    loadGaps()
  }, [loadGaps, answers])

  // 构造 5 领域数据
  const gapData = useMemo(() => {
    const targets: Record<string, number> = {
      '算法': 80,
      '系统设计': 70,
      'C/C++': 75,
      '数据库': 65,
      '计算机网络': 60
    }
    const mapping: Record<string, keyof typeof gaps> = {
      '算法': 'algorithm',
      '系统设计': 'system',
      'C/C++': 'cpp',
      '数据库': 'database',
      '计算机网络': 'network'
    }
    return Object.entries(targets).map(([name, target]) => {
      const key = mapping[name]
      const current = (gaps && gaps[key]?.level) || 0
      return { name, target, current }
    })
  }, [gaps])

  const totalCurrent = Math.round(
    gapData.reduce((acc, d) => acc + d.current, 0) / gapData.length
  )

  // === 空状态 ===
  if (!hasRealData) {
    return (
      <View className="gap-page">
        <View className="gap-header">
          <Text className="gap-title">📊 能力差距分析</Text>
          <Text className="gap-subtitle">对比目标岗位要求与当前能力</Text>
        </View>

        <View className="empty-state">
          <Text className="empty-emoji">🌱</Text>
          <Text className="empty-title">还没有学习数据</Text>
          <Text className="empty-desc">
            开始答题后，这里会自动分析你在算法、系统设计、C/C++、数据库、计算机网络
            {'\n'}5 大领域的掌握度，并对比目标水平给出差距和建议。
          </Text>

          <View className="empty-stats">
            <View className="empty-stat">
              <Text className="empty-stat-num">0</Text>
              <Text className="empty-stat-label">已答题数</Text>
            </View>
            <View className="empty-stat">
              <Text className="empty-stat-num">{nodes?.length || 0}</Text>
              <Text className="empty-stat-label">知识节点</Text>
            </View>
            <View className="empty-stat">
              <Text className="empty-stat-num">5</Text>
              <Text className="empty-stat-label">分析领域</Text>
            </View>
          </View>

          <View className="empty-cta" onClick={() => navigateTo('/quiz')}>
            <Text>🚀 开始答题，激活分析</Text>
          </View>

          <View className="empty-tip">
            <Text>💡 提示：完成 5 道以上题目后即可查看你的能力雷达</Text>
          </View>
        </View>
      </View>
    )
  }

  // === 正常状态 ===
  return (
    <View className="gap-page">
      <View className="gap-header">
        <Text className="gap-title">📊 能力差距分析</Text>
        <Text className="gap-subtitle">对比目标岗位要求与当前能力</Text>
      </View>

      <View className="gap-content">
        {/* 总体概览 */}
        <View className="overview-card">
          <View className="overview-header">
            <Text className="overview-title">总体进度</Text>
            <Text className="overview-meta">基于 {answers.length} 次答题</Text>
          </View>
          <View className="overview-progress">
            <View className="progress-circle">
              <Text className="progress-percent">{totalCurrent}%</Text>
              <Text className="progress-label">当前</Text>
            </View>
            <View className="progress-target">
              <Text className="target-value">70%</Text>
              <Text className="target-label">目标</Text>
            </View>
          </View>
        </View>

        {/* 领域差距列表 */}
        <View className="gap-list">
          {gapData.map((domain) => {
            const gap = Math.max(0, domain.target - domain.current)
            const gapPercent = domain.target > 0 ? (gap / domain.target) * 100 : 0
            const isUrgent = gap > 20

            return (
              <View
                key={domain.name}
                className={`gap-item ${isUrgent ? 'gap-urgent' : ''}`}
              >
                <View className="gap-item-header">
                  <Text className="domain-name">{domain.name}</Text>
                  <Text className={`gap-value ${isUrgent ? 'gap-warning' : ''}`}>
                    差距: {gap}分
                  </Text>
                </View>

                <View className="progress-bar">
                  <View className="progress-track">
                    <View
                      className="progress-fill current"
                      style={{ width: `${domain.current}%` }}
                    />
                    <View
                      className="progress-fill target"
                      style={{ left: `${domain.target}%` }}
                    />
                  </View>
                  <View className="progress-labels">
                    <Text className="label-current">{domain.current}%</Text>
                    <Text className="label-target">{domain.target}%</Text>
                  </View>
                </View>

                {isUrgent && (
                  <View className="gap-suggestion">
                    <Text className="suggestion-icon">📚</Text>
                    <Text className="suggestion-text">
                      建议优先学习该领域的基础知识
                    </Text>
                  </View>
                )}
              </View>
            )
          })}
        </View>

        {/* 学习建议 */}
        <View className="suggestions-card">
          <Text className="suggestions-title">学习建议</Text>
          <View className="suggestions-list">
            <View className="suggestion-item">
              <Text className="sug-number">1</Text>
              <Text className="sug-text">每天刷 2-3 道算法题，重点关注薄弱领域</Text>
            </View>
            <View className="suggestion-item">
              <Text className="sug-number">2</Text>
              <Text className="sug-text">系统学习分布式系统设计模式</Text>
            </View>
            <View className="suggestion-item">
              <Text className="sug-number">3</Text>
              <Text className="sug-text">完成 C++ 核心特性深度学习</Text>
            </View>
          </View>
        </View>
      </View>
    </View>
  )
}

export default GapAnalysis