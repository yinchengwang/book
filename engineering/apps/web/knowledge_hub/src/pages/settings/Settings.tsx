/**
 * 设置页面
 * - 主题切换（联动 document.documentElement.dataset.theme）
 * - API 密钥：显示/隐藏切换 + 一键复制
 * - API 地址/模型：明文
 * - 通知/数据管理/关于
 */
import { useState, useEffect } from 'react'
import { View, Text, Switch, Input, Button, Textarea } from '@tarojs/components'
import { useSettingsStore } from '@/stores/settingsStore'
import { useUserStore } from '@/stores/userStore'
import './Settings.scss'

const Settings = () => {
  const {
    theme,
    apiConfig,
    notifications,
    setTheme,
    setApiConfig,
    setNotifications,
    exportData,
    importData,
    clearData
  } = useSettingsStore()

  const user = useUserStore(s => s.user)
  const setName = useUserStore(s => s.setName)
  const setEmail = useUserStore(s => s.setEmail)
  const setBio = useUserStore(s => s.setBio)

  // API key 显示状态
  const [showApiKey, setShowApiKey] = useState(false)
  const [copyStatus, setCopyStatus] = useState<'idle' | 'success' | 'failed'>('idle')

  // 用户信息编辑
  const [editName, setEditName] = useState(user.name)
  const [editEmail, setEditEmail] = useState(user.email || '')
  const [editBio, setEditBio] = useState(user.bio || '')

  useEffect(() => {
    setEditName(user.name)
    setEditEmail(user.email || '')
    setEditBio(user.bio || '')
  }, [user.name, user.email, user.bio])

  // 主题切换时实时更新（useThemeEffect 在 Layout 已处理，但这里也打 log 便于调试）
  useEffect(() => {
    console.log('[Settings] 主题切换为:', theme)
    document.documentElement.dataset.theme = theme
  }, [theme])

  // 复制 API key
  const handleCopyApiKey = async () => {
    if (!apiConfig.apiKey) {
      setCopyStatus('failed')
      setTimeout(() => setCopyStatus('idle'), 1500)
      return
    }
    try {
      // 优先用 navigator.clipboard（HTTPS / localhost）
      if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(apiConfig.apiKey)
      } else {
        // 降级到 execCommand
        const ta = document.createElement('textarea')
        ta.value = apiConfig.apiKey
        ta.style.position = 'fixed'
        ta.style.opacity = '0'
        document.body.appendChild(ta)
        ta.select()
        document.execCommand('copy')
        document.body.removeChild(ta)
      }
      setCopyStatus('success')
      console.log('[Settings] API key 已复制')
    } catch (e) {
      console.error('[Settings] 复制失败:', e)
      setCopyStatus('failed')
    }
    setTimeout(() => setCopyStatus('idle'), 1500)
  }

  // 保存用户信息
  const handleSaveUser = () => {
    if (editName.trim()) setName(editName.trim())
    setEmail(editEmail.trim())
    setBio(editBio.trim())
  }

  return (
    <View className="settings-page">
      <View className="settings-header">
        <Text className="settings-title">⚙️ 设置</Text>
        <Text className="settings-subtitle">个性化配置</Text>
      </View>

      {/* 主题设置 */}
      <View className="settings-section">
        <View className="section-title">外观</View>
        <View className="setting-item">
          <View className="setting-info">
            <Text className="setting-label">深色模式</Text>
            <Text className="setting-desc">开启深色主题，护眼且更专注</Text>
          </View>
          <Switch
            checked={theme === 'dark'}
            onChange={(e) => setTheme(e.detail.value ? 'dark' : 'light')}
          />
        </View>
        <View className="theme-preview">
          <View className={`theme-card ${theme === 'dark' ? 'active' : ''}`} onClick={() => setTheme('dark')}>
            <View className="theme-card-inner dark-preview">
              <View className="dot" />
              <View className="line" />
              <View className="line short" />
            </View>
            <Text>深色</Text>
          </View>
          <View className={`theme-card ${theme === 'light' ? 'active' : ''}`} onClick={() => setTheme('light')}>
            <View className="theme-card-inner light-preview">
              <View className="dot" />
              <View className="line" />
              <View className="line short" />
            </View>
            <Text>浅色</Text>
          </View>
        </View>
      </View>

      {/* 用户资料 */}
      <View className="settings-section">
        <View className="section-title">用户资料</View>
        <View className="setting-item vertical">
          <Text className="setting-label">昵称</Text>
          <Input
            className="setting-input"
            type="text"
            value={editName}
            onInput={(e: any) => setEditName(e.detail.value)}
            placeholder="输入你的昵称"
          />
        </View>
        <View className="setting-item vertical">
          <Text className="setting-label">邮箱</Text>
          <Input
            className="setting-input"
            type="text"
            value={editEmail}
            onInput={(e: any) => setEditEmail(e.detail.value)}
            placeholder="your@email.com"
          />
        </View>
        <View className="setting-item vertical">
          <Text className="setting-label">个人简介</Text>
          <Textarea
            className="setting-textarea"
            value={editBio}
            onInput={(e: any) => setEditBio(e.detail.value)}
            placeholder="一句话介绍你自己"
          />
        </View>
        <View className="setting-actions">
          <Button className="action-btn primary" onClick={handleSaveUser}>
            保存资料
          </Button>
        </View>
      </View>

      {/* API 配置 */}
      <View className="settings-section">
        <View className="section-title">AI 配置</View>
        <View className="setting-item vertical">
          <View className="setting-label-row">
            <Text className="setting-label">API 密钥</Text>
            <View className="label-actions">
              <View
                className="mini-btn"
                onClick={() => setShowApiKey(!showApiKey)}
              >
                {showApiKey ? '🙈 隐藏' : '👁️ 显示'}
              </View>
              <View
                className={`mini-btn ${copyStatus === 'success' ? 'mini-btn-success' : ''} ${copyStatus === 'failed' ? 'mini-btn-failed' : ''}`}
                onClick={handleCopyApiKey}
              >
                {copyStatus === 'success' ? '✓ 已复制' : copyStatus === 'failed' ? '✗ 失败' : '📋 复制'}
              </View>
            </View>
          </View>
          <View className="input-with-icon">
            <Input
              className="setting-input"
              type={showApiKey ? 'text' : 'password' as any}
              placeholder="输入你的 API 密钥"
              value={apiConfig.apiKey}
              onInput={(e: any) => setApiConfig({ ...apiConfig, apiKey: e.detail.value })}
            />
          </View>
        </View>
        <View className="setting-item vertical">
          <Text className="setting-label">API 地址</Text>
          <Input
            className="setting-input"
            type="text"
            placeholder="https://api.example.com/v1"
            value={apiConfig.apiUrl}
            onInput={(e: any) => setApiConfig({ ...apiConfig, apiUrl: e.detail.value })}
          />
        </View>
        <View className="setting-item vertical">
          <Text className="setting-label">模型名称</Text>
          <Input
            className="setting-input"
            type="text"
            placeholder="gpt-4"
            value={apiConfig.model}
            onInput={(e: any) => setApiConfig({ ...apiConfig, model: e.detail.value })}
          />
        </View>
      </View>

      {/* 通知设置 */}
      <View className="settings-section">
        <View className="section-title">通知</View>
        <View className="setting-item">
          <View className="setting-info">
            <Text className="setting-label">复习提醒</Text>
            <Text className="setting-desc">到时通过站内通知提醒复习</Text>
          </View>
          <Switch
            checked={notifications.reviewReminder}
            onChange={(e: any) => setNotifications({ ...notifications, reviewReminder: e.detail.value })}
          />
        </View>
        <View className="setting-item">
          <View className="setting-info">
            <Text className="setting-label">面试提醒</Text>
            <Text className="setting-desc">面试前一天通过站内通知提醒</Text>
          </View>
          <Switch
            checked={notifications.interviewReminder}
            onChange={(e: any) => setNotifications({ ...notifications, interviewReminder: e.detail.value })}
          />
        </View>
      </View>

      {/* 数据管理 */}
      <View className="settings-section">
        <View className="section-title">数据管理</View>
        <View className="data-actions">
          <Button className="action-btn export" onClick={exportData}>
            📥 导出数据
          </Button>
          <Button className="action-btn import" onClick={importData}>
            📤 导入数据
          </Button>
          <Button className="action-btn danger" onClick={clearData}>
            🗑️ 清空数据
          </Button>
        </View>
      </View>

      {/* 关于 */}
      <View className="settings-section">
        <View className="section-title">关于</View>
        <View className="about-info">
          <Text className="app-name">Reading Radar 2.0</Text>
          <Text className="app-version">版本 1.0.0</Text>
          <Text className="app-desc">技术学习追踪平台</Text>
        </View>
      </View>
    </View>
  )
}

export default Settings