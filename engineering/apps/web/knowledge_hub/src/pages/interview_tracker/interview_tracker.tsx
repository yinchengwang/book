import { Component, useState } from 'react'
import { View, Text, Button, Input, Textarea } from '@tarojs/components'
import { useInterviewStore } from '@/stores/interviewStore'
import './interview_tracker.scss'

// 面试追踪页面
const InterviewTracker = () => {
  const {
    companies,
    totalInterviews,
    upcomingInterviews,
    addCompany,
    removeCompany,
    updateStatus,
    addInterviewRecord
  } = useInterviewStore()

  const [showForm, setShowForm] = useState(false)
  const [selectedCompany, setSelectedCompany] = useState<any>(null)
  const [formData, setFormData] = useState({
    name: '',
    position: '',
    status: '投递' as const,
    salary: '',
    notes: ''
  })

  // 状态颜色映射
  const statusColors: Record<string, string> = {
    '投递': '#6366f1',
    '笔试': '#f59e0b',
    '一面': '#3b82f6',
    '二面': '#8b5cf6',
    '三面': '#ec4899',
    'HR': '#10b981',
    'offer': '#22c55e',
    '拒绝': '#ef4444'
  }

  // 状态进度映射
  const statusProgress: Record<string, number> = {
    '投递': 1,
    '笔试': 2,
    '一面': 3,
    '二面': 4,
    '三面': 5,
    'HR': 6,
    'offer': 7,
    '拒绝': 0
  }

  // 处理新增公司
  const handleAddCompany = () => {
    if (!formData.name.trim()) return
    addCompany({
      id: `company-${Date.now()}`,
      name: formData.name,
      position: formData.position,
      status: formData.status,
      salary: formData.salary,
      notes: formData.notes,
      timeline: [],
      createdAt: new Date().toISOString()
    })
    setFormData({ name: '', position: '', status: '投递', salary: '', notes: '' })
    setShowForm(false)
  }

  // 处理状态切换
  const handleStatusChange = (companyId: string, newStatus: string) => {
    updateStatus(companyId, newStatus as any)
  }

  // 处理删除
  const handleRemove = (companyId: string) => {
    if (confirm('确定删除这条记录？')) {
      removeCompany(companyId)
    }
  }

  // 添加面试记录
  const handleAddRecord = (companyId: string, record: { date: string; content: string; type: '一面' | '二面' | '三面' | 'HR' | '笔试' }) => {
    addInterviewRecord(companyId, record)
  }

  // 分享公司信息
  const shareCompany = (company: any) => {
    // 生成面经 Markdown
    const markdown = `# ${company.name} 面经

## 基本信息
- **岗位**: ${company.position}
- **状态**: ${company.status}
${company.salary ? `- **薪资**: ${company.salary}` : ''}
- **记录时间**: ${company.createdAt}

## 面试进度
${['投递', '笔试', '一面', '二面', '三面', 'HR', 'offer'].map(s => {
  const isReached = statusProgress[company.status] >= statusProgress[s]
  return `- [${isReached ? 'x' : ' '}] ${s}`
}).join('\n')}

## 面试记录

${company.timeline?.map((r: any) => `### ${r.date} - ${r.type}
${r.content}
`).join('\n---\n\n') || '暂无面试记录'}

${company.notes ? `## 备注
${company.notes}
` : ''}

---
*由 Reading Radar 生成*
`

    // 复制到剪贴板
    // #ifdef H5
    navigator.clipboard.writeText(markdown).then(() => {
      // #ifdef MP-WEIXIN
      wx.showToast({ title: '已复制到剪贴板', icon: 'success' })
      // #endif
      // #ifdef H5
      alert('面经已复制到剪贴板！')
      // #endif
    }).catch(() => {
      // 降级：创建下载
      const blob = new Blob([markdown], { type: 'text/markdown' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `${company.name}-面经.md`
      a.click()
      URL.revokeObjectURL(url)
    })
    // #endif
  }

  // 分享到社区
  const shareToCommunity = (company: any) => {
    const shareContent = {
      title: `【面经分享】${company.name} - ${company.position}`,
      content: `面试进度：${company.status}\n\n面试记录：\n${company.timeline?.map((r: any) => `• ${r.date} ${r.type}: ${r.content}`).join('\n') || '暂无'}\n\n#面经 #${company.name} #求职`,
      tags: ['面经', company.name, company.position],
      questions: company.timeline?.filter((r: any) => r.content.includes('问题')).map((r: any) => r.content) || []
    }

    // 实际实现时调用分享 API
    console.log('分享到社区:', shareContent)
    // #ifdef MP-WEIXIN
    wx.showToast({ title: '分享成功', icon: 'success' })
    // #endif
    // #ifdef H5
    alert(`面经分享内容已准备好！\n\n标题: ${shareContent.title}\n内容: ${shareContent.content.substring(0, 50)}...`)
    // #endif
  }

  return (
    <View className="tracker-page">
      {/* 头部 */}
      <View className="tracker-header">
        <Text className="tracker-title">面试追踪</Text>
        <View className="tracker-stats">
          <View className="stat-item">
            <Text className="stat-value">{totalInterviews}</Text>
            <Text className="stat-label">总记录</Text>
          </View>
          <View className="stat-item highlight">
            <Text className="stat-value">{upcomingInterviews.length}</Text>
            <Text className="stat-label">待面试</Text>
          </View>
        </View>
      </View>

      {/* 待面试提醒 */}
      {upcomingInterviews.length > 0 && (
        <View className="upcoming-alert">
          <Text className="alert-icon">🔔</Text>
          <Text className="alert-text">
            你有 {upcomingInterviews.length} 场面试即将到来！
          </Text>
        </View>
      )}

      {/* 新增按钮 */}
      <Button className="add-btn" onClick={() => setShowForm(!showForm)}>
        {showForm ? '取消' : '+ 添加公司'}
      </Button>

      {/* 新增表单 */}
      {showForm && (
        <View className="add-form">
          <View className="form-item">
            <Text className="form-label">公司名称 *</Text>
            <Input
              className="form-input"
              placeholder="输入公司名称"
              value={formData.name}
              onInput={(e) => setFormData({ ...formData, name: e.detail.value })}
            />
          </View>
          <View className="form-item">
            <Text className="form-label">岗位</Text>
            <Input
              className="form-input"
              placeholder="输入岗位名称"
              value={formData.position}
              onInput={(e) => setFormData({ ...formData, position: e.detail.value })}
            />
          </View>
          <View className="form-item">
            <Text className="form-label">当前状态</Text>
            <View className="status-select">
              {['投递', '笔试', '一面', '二面', '三面', 'HR', 'offer', '拒绝'].map((s) => (
                <View
                  key={s}
                  className={`status-chip ${formData.status === s ? 'active' : ''}`}
                  style={{ background: formData.status === s ? statusColors[s] : 'transparent' }}
                  onClick={() => setFormData({ ...formData, status: s as any })}
                >
                  <Text className="chip-text">{s}</Text>
                </View>
              ))}
            </View>
          </View>
          <Button className="submit-btn" onClick={handleAddCompany}>
            保存
          </Button>
        </View>
      )}

      {/* 公司列表 */}
      <View className="company-list">
        {companies.length === 0 ? (
          <View className="empty-state">
            <Text className="empty-icon">📋</Text>
            <Text className="empty-text">暂无面试记录</Text>
            <Text className="empty-sub">点击上方按钮添加你的第一个公司</Text>
          </View>
        ) : (
          companies.map((company) => (
            <View key={company.id} className="company-card">
              <View className="card-header">
                <View className="company-info">
                  <Text className="company-name">{company.name}</Text>
                  <Text className="company-position">{company.position}</Text>
                </View>
                <View className="card-actions">
                  <Button className="action-btn share" onClick={() => shareCompany(company)}>
                    分享
                  </Button>
                  <Button className="action-btn delete" onClick={() => handleRemove(company.id)}>
                    删除
                  </Button>
                </View>
              </View>

              {/* 状态进度条 */}
              <View className="status-progress">
                {['投递', '笔试', '一面', '二面', '三面', 'HR', 'offer'].map((s, idx) => (
                  <View key={s} className="progress-step">
                    <View
                      className={`step-dot ${statusProgress[company.status] >= idx + 1 ? 'active' : ''}`}
                      style={{ background: statusProgress[company.status] >= idx + 1 ? statusColors[s] : '#374151' }}
                    />
                    <Text className="step-label">{s}</Text>
                  </View>
                ))}
              </View>

              {/* 状态切换 */}
              <View className="status-buttons">
                {['投递', '笔试', '一面', '二面', '三面', 'HR', 'offer'].map((s) => (
                  <View
                    key={s}
                    className={`status-btn ${company.status === s ? 'current' : ''}`}
                    style={{ background: company.status === s ? statusColors[s] : 'rgba(255,255,255,0.1)' }}
                    onClick={() => handleStatusChange(company.id, s)}
                  >
                    <Text className="btn-text">{s}</Text>
                  </View>
                ))}
              </View>

              {/* 面试记录时间线 */}
              {company.timeline && company.timeline.length > 0 && (
                <View className="timeline">
                  <Text className="timeline-title">面试记录</Text>
                  {company.timeline.map((record: any, idx: number) => (
                    <View key={idx} className="timeline-item">
                      <View className="timeline-dot" />
                      <View className="timeline-content">
                        <Text className="timeline-date">{record.date}</Text>
                        <Text className="timeline-text">{record.content}</Text>
                      </View>
                    </View>
                  ))}
                </View>
              )}

              {/* 添加记录 */}
              <View className="add-record">
                <Input
                  className="record-input"
                  placeholder="添加面试记录 (日期: 内容)"
                  onBlur={(e) => {
                    const value = e.detail.value
                    if (value.includes(':')) {
                      const [date, content] = value.split(':')
                      handleAddRecord(company.id, { date: date.trim(), content: content.trim(), type: '一面' })
                    }
                  }}
                />
              </View>
            </View>
          ))
        )}
      </View>
    </View>
  )
}

export default InterviewTracker
