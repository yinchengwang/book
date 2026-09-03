/**
 * 顶部 TopBar - PC 端
 * 搜索 + 通知 + 主题切换 + 用户下拉
 */
import { useState, useRef, useEffect } from 'react'
import { useNavigate } from 'react-router-dom'
import { useUserStore } from '@/stores/userStore'
import { useSettingsStore } from '@/stores/settingsStore'
import {
  useNotificationStore,
  timeAgo,
  Notification
} from '@/stores/notificationStore'
import { useReviewStore } from '@/stores/reviewStore'
import { useFiveYearPlanStore } from '@/stores/fiveYearPlanStore'
import { useInterviewStore } from '@/stores/interviewStore'
import { useQuizStore } from '@/stores/quizStore'
import './TopBar.scss'

export default function TopBar() {
  const navigate = useNavigate()
  const user = useUserStore(s => s.user)
  const setName = useUserStore(s => s.setName)
  const theme = useSettingsStore(s => s.theme)
  const setTheme = useSettingsStore(s => s.setTheme)

  const notifications = useNotificationStore(s => s.items)
  const unreadCount = useNotificationStore(s => s.unreadCount)
  const setNotifications = useNotificationStore(s => s.setItems)
  const markRead = useNotificationStore(s => s.markRead)
  const markAllRead = useNotificationStore(s => s.markAllRead)

  const dueReviews = useReviewStore(s => s.dueReviews)
  const interviewStore = useInterviewStore()
  const quizStore = useQuizStore()
  const fiveYearStreak = useFiveYearPlanStore(s => s.getStreak())

  const [search, setSearch] = useState('')
  const [showNotifications, setShowNotifications] = useState(false)
  const [showUserMenu, setShowUserMenu] = useState(false)
  const [editingName, setEditingName] = useState(false)
  const [draftName, setDraftName] = useState(user.name)
  const notifRef = useRef<HTMLDivElement>(null)
  const userRef = useRef<HTMLDivElement>(null)

  // 动态生成通知
  useEffect(() => {
    const items: Notification[] = []

    // 1. 复习提醒
    if (dueReviews.length > 0) {
      items.push({
        id: 'review-due',
        type: 'review',
        title: `今日有 ${dueReviews.length} 张卡片待复习`,
        desc: '点击开始今日复习',
        time: timeAgo(Date.now() - 2 * 3600 * 1000),
        timestamp: Date.now() - 2 * 3600 * 1000,
        link: '/review',
        icon: '📝',
        iconBg: 'rgba(99,102,241,0.2)',
        read: false
      })
    }

    // 2. 错题提醒（最近测验错题数）
    const wrongCount = quizStore.answers ? quizStore.answers.filter((a: any) => !a.correct).length : 0
    if (wrongCount > 0) {
      items.push({
        id: 'wrongbook',
        type: 'wrongbook',
        title: `错题本有 ${wrongCount} 道题`,
        desc: '建议及时复习巩固',
        time: timeAgo(Date.now() - 5 * 3600 * 1000),
        timestamp: Date.now() - 5 * 3600 * 1000,
        link: '/quiz',
        icon: '❌',
        iconBg: 'rgba(239,68,68,0.2)',
        read: false
      })
    }

    // 3. 面试追踪待办
    const trackerCount = interviewStore.companies
      ? interviewStore.companies.filter((c: any) =>
          ['hr', 'tech-1', 'tech-2', 'tech-3', 'offer'].includes(c.stage)).length
      : 0
    if (trackerCount > 0) {
      items.push({
        id: 'interview-active',
        type: 'interview',
        title: `${trackerCount} 家公司面试进行中`,
        desc: '查看面试追踪',
        time: timeAgo(Date.now() - 8 * 3600 * 1000),
        timestamp: Date.now() - 8 * 3600 * 1000,
        link: '/interview-tracker',
        icon: '💼',
        iconBg: 'rgba(16,185,129,0.2)',
        read: false
      })
    }

    // 4. 连续学习庆祝
    if (fiveYearStreak > 0) {
      items.push({
        id: 'streak',
        type: 'streak',
        title: `连续学习 ${fiveYearStreak} 天${fiveYearStreak >= 7 ? '，新纪录！' : ''}`,
        desc: '保持节奏，继续加油',
        time: timeAgo(Date.now() - 24 * 3600 * 1000),
        timestamp: Date.now() - 24 * 3600 * 1000,
        link: '/five-year-plan',
        icon: '🔥',
        iconBg: 'rgba(245,158,11,0.2)',
        read: true
      })
    }

    setNotifications(items)
  }, [dueReviews.length, interviewStore.companies, fiveYearStreak])

  // 点外部关闭 dropdown
  useEffect(() => {
    const onClick = (e: MouseEvent) => {
      if (notifRef.current && !notifRef.current.contains(e.target as Node)) {
        setShowNotifications(false)
      }
      if (userRef.current && !userRef.current.contains(e.target as Node)) {
        setShowUserMenu(false)
        setEditingName(false)
      }
    }
    document.addEventListener('mousedown', onClick)
    return () => document.removeEventListener('mousedown', onClick)
  }, [])

  // 切换主题
  const toggleTheme = () => {
    setTheme(theme === 'dark' ? 'light' : 'dark')
  }

  // 搜索跳转
  const handleSearch = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter' && search.trim()) {
      navigate(`/quiz?q=${encodeURIComponent(search.trim())}`)
      setSearch('')
    }
  }

  // 通知点击
  const handleNotifClick = (n: Notification) => {
    markRead(n.id)
    if (n.link) {
      navigate(n.link)
      setShowNotifications(false)
    }
  }

  // 保存昵称
  const saveName = () => {
    if (draftName.trim()) {
      setName(draftName.trim())
    }
    setEditingName(false)
  }

  return (
    <div className="topbar">
      {/* 搜索框 */}
      <div className="search">
        <span className="search-ic">🔍</span>
        <input
          type="text"
          placeholder="搜索知识点、题目、摘抄..."
          value={search}
          onChange={(e) => setSearch(e.target.value)}
          onKeyDown={handleSearch}
        />
        <span className="search-kbd">⌘ K · Enter</span>
      </div>

      {/* 右侧操作区 */}
      <div className="topbar-actions">
        {/* 通知 */}
        <div
          ref={notifRef}
          className={`icon-btn ${unreadCount > 0 ? 'has-badge' : ''}`}
          onClick={() => setShowNotifications(!showNotifications)}
          title="通知"
        >
          🔔
          {unreadCount > 0 && <span className="dot">{unreadCount}</span>}
        </div>

        {/* 主题切换 */}
        <div
          className="icon-btn theme-btn"
          onClick={toggleTheme}
          title={theme === 'dark' ? '切换到浅色主题' : '切换到深色主题'}
        >
          {theme === 'dark' ? '🌙' : '☀️'}
        </div>

        {/* 收藏 */}
        <div
          className="icon-btn"
          onClick={() => navigate('/excerpt')}
          title="我的摘抄收藏"
        >
          ⭐
        </div>

        {/* 用户下拉 */}
        <div ref={userRef} className="user-chip" onClick={() => setShowUserMenu(!showUserMenu)}>
          <div className="avatar">{user.avatar}</div>
          <span className="name">{user.name}</span>
          <span className="caret">▾</span>

          {showUserMenu && (
            <div className="user-menu" onClick={(e) => e.stopPropagation()}>
              <div className="user-menu-header">
                <div className="avatar-lg">{user.avatar}</div>
                {editingName ? (
                  <div className="name-edit">
                    <input
                      type="text"
                      value={draftName}
                      onChange={(e) => setDraftName(e.target.value)}
                      onKeyDown={(e) => e.key === 'Enter' && saveName()}
                      autoFocus
                    />
                    <div className="edit-actions">
                      <button onClick={saveName} className="btn-save">保存</button>
                      <button onClick={() => { setEditingName(false); setDraftName(user.name) }} className="btn-cancel">取消</button>
                    </div>
                  </div>
                ) : (
                  <div className="user-info">
                    <div className="user-name">{user.name}</div>
                    <div className="user-meta">加入于 {new Date(user.joinDate).toLocaleDateString('zh-CN')}</div>
                  </div>
                )}
              </div>

              <div className="user-menu-divider" />

              <div className="user-menu-item" onClick={() => { setEditingName(true); setDraftName(user.name) }}>
                <span className="mi">✏️</span>
                <span>编辑昵称</span>
              </div>

              <div className="user-menu-item" onClick={() => { navigate('/settings'); setShowUserMenu(false) }}>
                <span className="mi">⚙️</span>
                <span>设置</span>
              </div>

              <div className="user-menu-item" onClick={() => { navigate('/excerpt'); setShowUserMenu(false) }}>
                <span className="mi">📚</span>
                <span>我的摘抄</span>
              </div>

              <div className="user-menu-item" onClick={() => { navigate('/interview-tracker'); setShowUserMenu(false) }}>
                <span className="mi">💼</span>
                <span>面试追踪</span>
              </div>

              <div className="user-menu-divider" />

              <div className="user-menu-item danger" onClick={() => {
                if (confirm('确定要清空所有数据吗？此操作不可恢复！')) {
                  localStorage.clear()
                  window.location.reload()
                }
              }}>
                <span className="mi">🗑️</span>
                <span>清空所有数据</span>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* 通知下拉 */}
      {showNotifications && (
        <div className="notification-dropdown" onClick={(e) => e.stopPropagation()}>
          <div className="notif-header">
            <span>通知 {notifications.length > 0 ? `(${notifications.length})` : ''}</span>
            {notifications.length > 0 && unreadCount > 0 && (
              <span className="notif-mark-all" onClick={markAllRead}>全部已读</span>
            )}
          </div>
          {notifications.length === 0 ? (
            <div className="notif-empty">
              <div className="emoji">🔕</div>
              <div>暂无通知</div>
              <div className="sub">完成学习和复习后，这里会出现相关提醒</div>
            </div>
          ) : (
            <div className="notif-list">
              {notifications.map(n => (
                <div
                  key={n.id}
                  className={`notif-item ${n.read ? 'read' : 'unread'}`}
                  onClick={() => handleNotifClick(n)}
                >
                  <div className="notif-ic" style={{ background: n.iconBg }}>{n.icon}</div>
                  <div className="notif-body">
                    <div className="notif-title">{n.title}</div>
                    {n.desc && <div className="notif-desc">{n.desc}</div>}
                    <div className="notif-time">{n.time}</div>
                  </div>
                  {!n.read && <div className="notif-unread-dot" />}
                </div>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  )
}