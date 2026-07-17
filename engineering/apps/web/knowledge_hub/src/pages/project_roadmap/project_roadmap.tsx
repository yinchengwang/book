import { Component, useState } from 'react'
import { View, Text, Button, Input, Textarea } from '@tarojs/components'
import { useProjectStore, Project } from '@/stores/projectStore'
import './project_roadmap.scss'

// 项目路线页面（预埋）
const ProjectRoadmap = () => {
  const {
    projects,
    addProject,
    updateProject,
    deleteProject
  } = useProjectStore()

  const [showForm, setShowForm] = useState(false)
  const [selectedProject, setSelectedProject] = useState<Project | null>(null)
  const [formData, setFormData] = useState({
    name: '',
    description: '',
    status: '规划中' as const,
    priority: '中' as const,
    tags: ''
  })

  // 状态颜色
  const statusColors: Record<string, string> = {
    '规划中': '#6366f1',
    '进行中': '#3b82f6',
    '已完成': '#10b981',
    '暂停': '#f59e0b',
    '废弃': '#ef4444'
  }

  // 优先级颜色
  const priorityColors: Record<string, string> = {
    '高': '#ef4444',
    '中': '#f59e0b',
    '低': '#10b981'
  }

  // 处理新增
  const handleAdd = () => {
    if (!formData.name.trim()) return
    addProject({
      id: `project-${Date.now()}`,
      name: formData.name,
      description: formData.description,
      status: formData.status,
      priority: formData.priority,
      tags: formData.tags.split(',').map(t => t.trim()).filter(Boolean),
      knowledgeLinks: [],
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString()
    })
    setFormData({ name: '', description: '', status: '规划中', priority: '中', tags: '' })
    setShowForm(false)
  }

  return (
    <View className="roadmap-page">
      {/* 头部 */}
      <View className="roadmap-header">
        <Text className="page-title">项目路线</Text>
        <Text className="page-subtitle">
          管理你的学习和实战项目
        </Text>
      </View>

      {/* 统计卡片 */}
      <View className="stats-row">
        <View className="stat-card">
          <Text className="stat-value">{projects.length}</Text>
          <Text className="stat-label">总项目</Text>
        </View>
        <View className="stat-card">
          <Text className="stat-value">
            {projects.filter(p => p.status === '进行中').length}
          </Text>
          <Text className="stat-label">进行中</Text>
        </View>
        <View className="stat-card">
          <Text className="stat-value">
            {projects.filter(p => p.status === '已完成').length}
          </Text>
          <Text className="stat-label">已完成</Text>
        </View>
      </View>

      {/* 添加按钮 */}
      <Button className="add-btn" onClick={() => setShowForm(!showForm)}>
        {showForm ? '取消' : '+ 新建项目'}
      </Button>

      {/* 新增表单 */}
      {showForm && (
        <View className="add-form">
          <View className="form-item">
            <Text className="form-label">项目名称 *</Text>
            <Input
              className="form-input"
              placeholder="输入项目名称"
              value={formData.name}
              onInput={(e) => setFormData({ ...formData, name: e.detail.value })}
            />
          </View>
          <View className="form-item">
            <Text className="form-label">描述</Text>
            <Textarea
              className="form-textarea"
              placeholder="项目描述..."
              value={formData.description}
              onInput={(e) => setFormData({ ...formData, description: e.detail.value })}
            />
          </View>
          <View className="form-row">
            <View className="form-item half">
              <Text className="form-label">状态</Text>
              <View className="select-options">
                {['规划中', '进行中', '已完成', '暂停'].map(s => (
                  <View
                    key={s}
                    className={`select-chip ${formData.status === s ? 'active' : ''}`}
                    style={{ borderColor: formData.status === s ? statusColors[s] : undefined }}
                    onClick={() => setFormData({ ...formData, status: s as any })}
                  >
                    <Text>{s}</Text>
                  </View>
                ))}
              </View>
            </View>
            <View className="form-item half">
              <Text className="form-label">优先级</Text>
              <View className="select-options">
                {['高', '中', '低'].map(p => (
                  <View
                    key={p}
                    className={`select-chip ${formData.priority === p ? 'active' : ''}`}
                    style={{ borderColor: formData.priority === p ? priorityColors[p] : undefined }}
                    onClick={() => setFormData({ ...formData, priority: p as any })}
                  >
                    <Text>{p}</Text>
                  </View>
                ))}
              </View>
            </View>
          </View>
          <View className="form-item">
            <Text className="form-label">关联知识（逗号分隔）</Text>
            <Input
              className="form-input"
              placeholder="如: 算法, 系统设计"
              value={formData.tags}
              onInput={(e) => setFormData({ ...formData, tags: e.detail.value })}
            />
          </View>
          <Button className="submit-btn" onClick={handleAdd}>
            创建项目
          </Button>
        </View>
      )}

      {/* 项目列表 */}
      <View className="project-list">
        {projects.length === 0 ? (
          <View className="empty-state">
            <Text className="empty-icon">🚀</Text>
            <Text className="empty-text">暂无项目</Text>
            <Text className="empty-sub">创建你的第一个学习项目吧</Text>
          </View>
        ) : (
          projects.map(project => (
            <View
              key={project.id}
              className="project-card"
              onClick={() => setSelectedProject(project)}
            >
              <View className="card-header">
                <View className="project-info">
                  <Text className="project-name">{project.name}</Text>
                  <View className="meta-row">
                    <View
                      className="status-badge"
                      style={{ background: statusColors[project.status] + '30' }}
                    >
                      <Text style={{ color: statusColors[project.status] }}>
                        {project.status}
                      </Text>
                    </View>
                    <View
                      className="priority-badge"
                      style={{ color: priorityColors[project.priority] }}
                    >
                      {project.priority === '高' && '🔴'}
                      {project.priority === '中' && '🟡'}
                      {project.priority === '低' && '🟢'}
                      <Text> {project.priority}</Text>
                    </View>
                  </View>
                </View>
                <Text className="arrow">›</Text>
              </View>

              {project.description && (
                <Text className="project-desc">{project.description}</Text>
              )}

              <View className="card-footer">
                <View className="tags">
                  {project.tags.slice(0, 3).map((tag, idx) => (
                    <View key={idx} className="tag">
                      <Text>{tag}</Text>
                    </View>
                  ))}
                  {project.tags.length > 3 && (
                    <Text className="more-tags">+{project.tags.length - 3}</Text>
                  )}
                </View>
                <Text className="date">
                  {new Date(project.updatedAt).toLocaleDateString('zh-CN')}
                </Text>
              </View>
            </View>
          ))
        )}
      </View>

      {/* 详情弹窗 */}
      {selectedProject && (
        <View className="modal-overlay" onClick={() => setSelectedProject(null)}>
          <View className="modal-content" onClick={(e) => e.stopPropagation()}>
            <View className="modal-header">
              <Text className="modal-title">{selectedProject.name}</Text>
              <Text className="close-btn" onClick={() => setSelectedProject(null)}>✕</Text>
            </View>

            <View className="modal-badges">
              <View
                className="status-badge"
                style={{ background: statusColors[selectedProject.status] + '30' }}
              >
                <Text style={{ color: statusColors[selectedProject.status] }}>
                  {selectedProject.status}
                </Text>
              </View>
              <Text style={{ color: priorityColors[selectedProject.priority] }}>
                {selectedProject.priority}优先级
              </Text>
            </View>

            {selectedProject.description && (
              <Text className="modal-desc">{selectedProject.description}</Text>
            )}

            <View className="modal-section">
              <Text className="section-title">关联知识</Text>
              <View className="tags">
                {selectedProject.tags.map((tag, idx) => (
                  <View key={idx} className="tag">
                    <Text>{tag}</Text>
                  </View>
                ))}
              </View>
            </View>

            <View className="modal-actions">
              <Button
                className="action-btn"
                onClick={() => {
                  updateProject(selectedProject.id, {
                    status: selectedProject.status === '进行中' ? '已完成' : '进行中'
                  })
                  setSelectedProject({
                    ...selectedProject,
                    status: selectedProject.status === '进行中' ? '已完成' : '进行中'
                  })
                }}
              >
                {selectedProject.status === '进行中' ? '标记完成' : '重新开始'}
              </Button>
              <Button
                className="action-btn danger"
                onClick={() => {
                  deleteProject(selectedProject.id)
                  setSelectedProject(null)
                }}
              >
                删除
              </Button>
            </View>
          </View>
        </View>
      )}
    </View>
  )
}

export default ProjectRoadmap
