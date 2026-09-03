/**
 * 图解系列 - 分类详情页（双栏布局）
 * 左侧：文章列表 | 右侧：文章内容
 */
import { useParams, useNavigate } from 'react-router-dom'
import { useState, useRef, useEffect } from 'react'
import { CATEGORIES, CategoryId, getArticlesByCategory, learnDeepList } from '@/data/learn_deep_index'
import { learnDeepContent } from '@/data/learn_deep_content'
import { MarkdownRenderer } from '@/components/markdown/MarkdownRenderer'
import './LearnDeepDetail.scss'

export default function LearnDeepDetail() {
  const { category } = useParams<{ category: string }>()
  const navigate = useNavigate()
  const [selectedSlug, setSelectedSlug] = useState<string | null>(null)
  const contentRef = useRef<HTMLDivElement>(null)

  const categoryId = category as CategoryId
  const categoryMeta = CATEGORIES.find(c => c.id === categoryId)
  const articles = getArticlesByCategory(learnDeepList, categoryId)
  const selectedArticle = articles.find(a => a.slug === selectedSlug)
  const content = selectedSlug ? learnDeepContent[selectedSlug] : null

  // 切换文章时，滚动到顶部
  useEffect(() => {
    if (contentRef.current) {
      contentRef.current.scrollTop = 0
    }
  }, [selectedSlug])

  // 分类变化时，重置选中文章
  useEffect(() => {
    setSelectedSlug(null)
  }, [categoryId])

  if (!categoryMeta) {
    return (
      <div className="ld-detail">
        <div className="ld-error">分类不存在</div>
      </div>
    )
  }

  return (
    <div className="ld-detail">
      {/* 顶部返回栏 */}
      <div className="ld-topbar">
        <button className="ld-back" onClick={() => navigate('/learn_deep')}>← 返回分类</button>
        <span className="ld-title">{categoryMeta.name}</span>
        <span className="ld-count">{articles.length} 篇</span>
      </div>

      <div className="ld-body">
        {/* 左侧文章列表 */}
        <div className="ld-sidebar">
          <div className="ld-sidebar-header">📚 文章列表</div>
          <div className="ld-article-list">
            {articles.map(article => (
              <button
                key={article.slug}
                className={`ld-article-item ${selectedSlug === article.slug ? 'active' : ''}`}
                onClick={() => setSelectedSlug(article.slug)}
              >
                {article.title}
              </button>
            ))}
          </div>
        </div>

        {/* 右侧文章内容 */}
        <div className="ld-content" ref={contentRef}>
          {selectedArticle && content ? (
            <div className="ld-reader">
              <div className="ld-reader-header">
                <h1>{selectedArticle.title}</h1>
                <p className="ld-excerpt">{selectedArticle.excerpt}</p>
              </div>
              <div className="ld-reader-body">
                <MarkdownRenderer content={content} />
              </div>
            </div>
          ) : (
            <div className="ld-placeholder">👈 从左侧选择文章阅读</div>
          )}
        </div>
      </div>
    </div>
  )
}