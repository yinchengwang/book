/**
 * 侧边栏导航 - PC 端
 */
import { useLocation, useNavigate } from 'react-router-dom'
import { useState, useEffect } from 'react'
import { NAV_GROUPS, NAV_ITEMS } from '../../config/routes'
import { useFiveYearPlanStore } from '@/stores/fiveYearPlanStore'
import './Sidebar.scss'

export default function Sidebar() {
  const location = useLocation()
  const navigate = useNavigate()
  const streak = useFiveYearPlanStore(s => s.getStreak())

  // 动态 badge：每隔 1s 重算（依赖 store 状态变化）
  const [badges, setBadges] = useState<Record<string, number | string | null>>({})

  useEffect(() => {
    const compute = () => {
      const next: Record<string, number | string | null> = {}
      for (const item of NAV_ITEMS) {
        try {
          if (item.badgeGetter) {
            const v = item.badgeGetter()
            next[item.path] = v === 0 ? null : v
          } else {
            next[item.path] = item.badge ?? null
          }
        } catch {
          next[item.path] = item.badge ?? null
        }
      }
      setBadges(next)
    }
    compute()
    const t = setInterval(compute, 1000)
    return () => clearInterval(t)
  }, [streak])

  const isActive = (path: string) => {
    if (path === '/') return location.pathname === '/'
    return location.pathname === path || location.pathname.startsWith(path + '/')
  }

  return (
    <aside className="sidebar">
      {/* 品牌 */}
      <div className="brand">
        <div className="brand-logo">📡</div>
        <div className="brand-info">
          <div className="brand-name">Reading Radar</div>
          <div className="brand-sub">v2.0 · Learning Tracker</div>
        </div>
      </div>

      {/* 导航分组 */}
      {Object.entries(NAV_GROUPS).map(([group, items]) => (
        <div key={group} className="nav-section">
          <div className="nav-title">{group}</div>
          {items.map((item) => {
            const badge = badges[item.path]
            return (
              <div
                key={item.path}
                className={`nav-item ${isActive(item.path) ? 'active' : ''}`}
                onClick={() => navigate(item.path)}
              >
                <span className="ic">{item.icon}</span>
                <span className="label">{item.title}</span>
                {badge !== null && badge !== undefined && badge !== '' && (
                  <span className={`badge ${typeof badge === 'string' && badge.startsWith('🔥') ? 'badge-fire' : ''}`}>
                    {badge}
                  </span>
                )}
              </div>
            )
          })}
        </div>
      ))}

      {/* 底部信息 - 动态 streak */}
      <div className="sidebar-footer">
        <div className={`footer-card ${streak === 0 ? 'footer-card-empty' : ''}`}>
          <div className="footer-emoji">{streak > 0 ? '🔥' : '🌱'}</div>
          <div className="footer-title">
            {streak > 0 ? `连续学习 ${streak} 天` : '开始你的学习旅程'}
          </div>
          <div className="footer-sub">
            {streak === 0 ? '点击任意功能开始' : streak >= 30 ? '太棒了！保持节奏' : '再接再厉'}
          </div>
        </div>
      </div>
    </aside>
  )
}