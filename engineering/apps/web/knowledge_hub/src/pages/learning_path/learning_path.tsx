/**
 * 学习路径页面
 *
 * - 路径选择 tab
 * - 路径概览（图标、描述、难度、预计时长、步骤数）
 * - 进度条（基于 sub-step 完成数）
 * - 步骤列表：每个 step 可展开/折叠 sub-step
 * - sub-step 标记完成（checkbox）
 * - 控制按钮（上一步 / 重置 / 下一步）
 */
import { useState } from 'react'
import { View, Text, Button } from '@tarojs/components'
import { useLearningPathStore, SubStepType } from '@/stores/learningPathStore'
import './learning_path.scss'

const DIFFICULTY_COLORS: Record<string, string> = {
  '入门': '#10b981',
  '初级': '#3b82f6',
  '中级': '#f59e0b',
  '高级': '#ef4444',
  '专家': '#8b5cf6'
}

const SUB_TYPE_ICONS: Record<SubStepType, string> = {
  knowledge: '📖',
  practice: '⚡',
  project: '🛠️'
}

const SUB_TYPE_LABEL: Record<SubStepType, string> = {
  knowledge: '知识点',
  practice: '实战',
  project: '项目'
}

const SUB_TYPE_COLORS: Record<SubStepType, string> = {
  knowledge: 'rgba(99, 102, 241, 0.25)',
  practice: 'rgba(245, 158, 11, 0.25)',
  project: 'rgba(16, 185, 129, 0.25)'
}

const LearningPath = () => {
  const {
    paths,
    activePath,
    currentStep,
    completedSubSteps,
    setActivePath,
    nextStep,
    prevStep,
    resetPath,
    toggleSubStep,
    getStepProgress,
    getPathProgress
  } = useLearningPathStore()

  const [showReset, setShowReset] = useState(false)
  const [expandedSteps, setExpandedSteps] = useState<Set<string>>(new Set())

  const toggleExpand = (stepId: string) => {
    setExpandedSteps(prev => {
      const next = new Set(prev)
      if (next.has(stepId)) next.delete(stepId)
      else next.add(stepId)
      return next
    })
  }

  const currentPathProgress = activePath ? getPathProgress(activePath.id) : { done: 0, total: 0, percent: 0 }

  // 兜底：老 v1 数据 activePath 残留时，filter 掉没有 subSteps 字段的 step
  const safeSteps = (activePath?.steps || []).filter(s => Array.isArray(s.subSteps))

  return (
    <View className="path-page">
      <View className="path-header">
        <Text className="path-title">🛤️ 学习路径</Text>
        <Text className="path-subtitle">系统化学习路线 · 大点拆小点 · 精细化学习</Text>
      </View>

      {/* 路径选择 */}
      <View className="path-selector">
        {paths.map((path) => (
          <View
            key={path.id}
            className={`path-tab ${activePath?.id === path.id ? 'active' : ''}`}
            onClick={() => setActivePath(path.id)}
          >
            <Text className="tab-icon">{path.icon}</Text>
            <View className="tab-info">
              <Text className="tab-name">{path.name}</Text>
              <Text className="tab-meta">{path.steps.length} 步 · {path.estimatedHours}h</Text>
            </View>
          </View>
        ))}
      </View>

      {activePath && (
        <View className="path-content">
          {/* 路径概览 */}
          <View className="path-overview">
            <View className="overview-header">
              <View className="overview-icon">{activePath.icon}</View>
              <View className="overview-info">
                <Text className="overview-name">{activePath.name}</Text>
                <Text className="overview-desc">{activePath.description}</Text>
              </View>
            </View>
            <View className="overview-stats">
              <View className="stat-badge">
                <Text className="badge-value">{safeSteps.length}</Text>
                <Text className="badge-label">大步骤</Text>
              </View>
              <View className="stat-badge">
                <Text className="badge-value">
                  {safeSteps.reduce((sum, s) => sum + s.subSteps.length, 0)}
                </Text>
                <Text className="badge-label">小任务</Text>
              </View>
              <View className="stat-badge">
                <Text className="badge-value">
                  {safeSteps.reduce((sum, s) => sum + s.estimatedHours, 0)}h
                </Text>
                <Text className="badge-label">总时长</Text>
              </View>
              <View className="stat-badge">
                <Text className="badge-value" style={{ color: DIFFICULTY_COLORS[activePath.difficulty] }}>
                  {activePath.difficulty}
                </Text>
                <Text className="badge-label">难度</Text>
              </View>
            </View>
          </View>

          {/* 进度条 - 基于 sub-step 完成 */}
          <View className="progress-section">
            <View className="progress-header">
              <Text className="progress-title">📊 学习进度</Text>
              <Text className="progress-percent">
                {currentPathProgress.done}/{currentPathProgress.total} · {currentPathProgress.percent}%
              </Text>
            </View>
            <View className="progress-track">
              <View
                className="progress-fill"
                style={{ width: `${currentPathProgress.percent}%` }}
              />
            </View>
            <Text className="progress-hint">
              💡 完成小任务即可推进进度，支持任意顺序
            </Text>
          </View>

          {/* 步骤列表 */}
          <View className="steps-list">
            <Text className="steps-title">学习步骤</Text>
            {safeSteps.map((step, idx) => {
              const isCompleted = idx < currentStep
              const isCurrent = idx === currentStep
              const stepProgress = getStepProgress(step.id)
              const isExpanded = expandedSteps.has(step.id)
              const allSubDone = stepProgress.total > 0 && stepProgress.done === stepProgress.total

              return (
                <View
                  key={step.id}
                  className={`step-card ${isCompleted ? 'completed' : ''} ${isCurrent ? 'current' : ''} ${isExpanded ? 'expanded' : ''}`}
                >
                  <View className="step-indicator">
                    <View className={`step-number ${isCompleted ? 'done' : ''} ${allSubDone ? 'all-done' : ''}`}>
                      {isCompleted ? '✓' : allSubDone ? '⭐' : idx + 1}
                    </View>
                    {idx < safeSteps.length - 1 && (
                      <View className={`step-line ${isCompleted ? 'done' : ''}`} />
                    )}
                  </View>

                  <View className="step-content">
                    <View
                      className="step-header clickable"
                      onClick={() => toggleExpand(step.id)}
                    >
                      <View className="step-title-wrap">
                        <Text className="step-name">{step.title}</Text>
                        <Text className="step-desc">{step.description}</Text>
                      </View>
                      <View className="step-meta">
                        <View className="step-tags">
                          <Text className="tag duration">{step.estimatedHours}h</Text>
                          <Text
                            className="tag"
                            style={{ background: DIFFICULTY_COLORS[step.difficulty] + '30' }}
                          >
                            {step.difficulty}
                          </Text>
                        </View>
                        <View className="expand-icon">{isExpanded ? '▼' : '▶'}</View>
                      </View>
                    </View>

                    {/* 步骤进度条 */}
                    {step.subSteps.length > 0 && (
                      <View className="step-progress-row">
                        <View className="mini-progress-track">
                          <View
                            className="mini-progress-fill"
                            style={{ width: `${stepProgress.percent}%` }}
                          />
                        </View>
                        <Text className="mini-progress-text">
                          {stepProgress.done}/{stepProgress.total}
                        </Text>
                      </View>
                    )}

                    {/* 折叠展开 sub-steps */}
                    {isExpanded && (
                      <View className="substeps-container">
                        {step.resources && step.resources.length > 0 && (
                          <View className="step-resources">
                            <Text className="resources-title">📚 推荐资源：</Text>
                            <View className="resources-list">
                              {step.resources.map((res, i) => (
                                <Text key={i} className="resource-item">• {res}</Text>
                              ))}
                            </View>
                          </View>
                        )}

                        <View className="substeps-list">
                          {step.subSteps.map(sub => {
                            const key = `${step.id}:${sub.id}`
                            const done = !!completedSubSteps[key]
                            return (
                              <View
                                key={sub.id}
                                className={`substep ${done ? 'done' : ''}`}
                                onClick={() => toggleSubStep(step.id, sub.id)}
                              >
                                <View className={`substep-check ${done ? 'checked' : ''}`}>
                                  <Text>{done ? '✓' : ''}</Text>
                                </View>
                                <View
                                  className="substep-type"
                                  style={{ background: SUB_TYPE_COLORS[sub.type] }}
                                >
                                  <Text>{SUB_TYPE_ICONS[sub.type]} {SUB_TYPE_LABEL[sub.type]}</Text>
                                </View>
                                <Text className="substep-title">{sub.title}</Text>
                                <Text className="substep-hours">{sub.estimatedHours}h</Text>
                              </View>
                            )
                          })}
                        </View>

                        {isCurrent && (
                          <View className="step-actions">
                            <Button
                              className="action-btn continue"
                              onClick={nextStep}
                            >
                              ✅ 完成本步骤，进入下一步
                            </Button>
                          </View>
                        )}
                      </View>
                    )}
                  </View>
                </View>
              )
            })}
          </View>

          {/* 控制按钮 */}
          <View className="path-controls">
            <Button
              className="control-btn"
              disabled={currentStep === 0}
              onClick={prevStep}
            >
              ← 上一步
            </Button>
            <Button
              className="control-btn reset"
              onClick={() => setShowReset(true)}
            >
              🔄 重置
            </Button>
            <Button
              className="control-btn"
              disabled={currentStep >= safeSteps.length - 1}
              onClick={nextStep}
            >
              下一步 →
            </Button>
          </View>

          {/* 重置确认 */}
          {showReset && (
            <View className="modal-overlay" onClick={() => setShowReset(false)}>
              <View className="modal-content" onClick={(e) => e.stopPropagation()}>
                <Text className="modal-title">⚠️ 确认重置？</Text>
                <Text className="modal-text">
                  这将清除「{activePath.name}」的所有学习进度和勾选状态。
                </Text>
                <View className="modal-actions">
                  <Button className="modal-btn cancel" onClick={() => setShowReset(false)}>
                    取消
                  </Button>
                  <Button className="modal-btn confirm" onClick={() => { resetPath(); setShowReset(false) }}>
                    确定重置
                  </Button>
                </View>
              </View>
            </View>
          )}
        </View>
      )}
    </View>
  )
}

export default LearningPath