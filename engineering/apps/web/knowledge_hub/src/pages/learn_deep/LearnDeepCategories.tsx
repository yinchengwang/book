/**
 * 图解系列 - 分类选择页面
 */
import { useNavigate } from 'react-router-dom'
import { CATEGORIES, getLearnDeepStats } from '@/data/learn_deep_index'
import './LearnDeepCategories.scss'

const CATEGORY_ICONS: Record<string, string> = {
  linux: '🐧', network: '🌐', system: '⚙️', database: '🗄️',
  vdb: '🔢', algorithm: '📊', c: '🔧', python: '🐍', cpp: '⚡'
}

export default function LearnDeepCategories() {
  const navigate = useNavigate()
  const stats = getLearnDeepStats()

  return (
    <div className="ld-categories">
      <h1>📖 图解系列</h1>
      <div className="ld-grid">
        {CATEGORIES.map(cat => {
          const stat = stats.find(s => s.id === cat.id)
          return (
            <button key={cat.id} className="ld-card" onClick={() => navigate(`/learn_deep/${cat.id}`)}>
              <span className="ld-icon">{CATEGORY_ICONS[cat.id]}</span>
              <span className="ld-name">{cat.name}</span>
              <span className="ld-count">{stat?.count || 0} 篇</span>
            </button>
          )
        })}
      </div>
    </div>
  )
}