/**
 * 摘抄管理页面
 *
 * - 展示老项目迁移的 149 条摘抄（来自 project/reading-radar/data/excerpt/）
 * - 支持按书籍/标签/年份筛选，搜索关键字
 * - 卡片显示年份、状态徽章、标签
 * - 详情弹窗：复制、删除、修改状态
 */
import { useState, useMemo } from 'react'
import { View, Text, Input, Button, Textarea } from '@tarojs/components'
import { useExcerptStore, useExcerptDerivedData, ReadStatus } from '@/stores/excerptStore'
import './Excerpt.scss'

const STATUS_LABEL: Record<ReadStatus, string> = {
  reading: '📖 在读',
  read: '✅ 已读',
  mastered: '⭐ 掌握',
  wishlist: '🔖 想读'
}

const STATUS_COLOR: Record<ReadStatus, string> = {
  reading: 'rgba(99, 102, 241, 0.25)',
  read: 'rgba(16, 185, 129, 0.2)',
  mastered: 'rgba(245, 158, 11, 0.25)',
  wishlist: 'rgba(168, 85, 247, 0.2)'
}

const Excerpt = () => {
  const {
    excerpts,
    filterBook,
    filterTag,
    filterYear,
    searchQuery,
    setFilterBook,
    setFilterTag,
    setFilterYear,
    setSearchQuery,
    addExcerpt,
    deleteExcerpt,
    toggleFavorite,
    updateNote,
    setStatus
  } = useExcerptStore()

  const { books, tags, years, stats } = useExcerptDerivedData()

  const [showAddForm, setShowAddForm] = useState(false)
  const [selectedExcerpt, setSelectedExcerpt] = useState<any>(null)
  const [editNote, setEditNote] = useState('')
  const [newExcerpt, setNewExcerpt] = useState({
    content: '',
    book: '',
    tags: '',
    note: ''
  })

  // 过滤后的摘抄
  const filteredExcerpts = useMemo(() => {
    return excerpts.filter(ex => {
      // 兜底：tags 非数组时按空数组处理
      const exTags = Array.isArray(ex.tags) ? ex.tags : []
      const matchBook = !filterBook || ex.book === filterBook
      const matchTag = !filterTag || exTags.includes(filterTag)
      const matchYear = !filterYear || ex.date === filterYear
      const matchSearch = !searchQuery ||
        ex.content.toLowerCase().includes(searchQuery.toLowerCase()) ||
        (ex.note && ex.note.toLowerCase().includes(searchQuery.toLowerCase())) ||
        ex.book.toLowerCase().includes(searchQuery.toLowerCase())
      return matchBook && matchTag && matchYear && matchSearch
    })
  }, [excerpts, filterBook, filterTag, filterYear, searchQuery])

  // 处理新增
  const handleAdd = () => {
    if (!newExcerpt.content.trim() || !newExcerpt.book.trim()) return
    addExcerpt({
      id: `excerpt-new-${Date.now()}`,
      content: newExcerpt.content,
      book: newExcerpt.book,
      tags: newExcerpt.tags.split(',').map(t => t.trim()).filter(Boolean),
      note: newExcerpt.note,
      favorite: false,
      createdAt: new Date().toISOString(),
      date: String(new Date().getFullYear())
    })
    setNewExcerpt({ content: '', book: '', tags: '', note: '' })
    setShowAddForm(false)
  }

  // 复制到剪贴板（兼容 H5 和小程序）
  const handleCopy = (text: string) => {
    if (navigator.clipboard) {
      navigator.clipboard.writeText(text).then(() => {
        console.log('[Excerpt] 摘抄已复制')
      }).catch((e) => {
        console.error('[Excerpt] 复制失败:', e)
      })
    } else if ((window as any).wx?.setClipboardData) {
      (window as any).wx.setClipboardData({ data: text })
    }
  }

  // 打开详情
  const openDetail = (ex: any) => {
    setSelectedExcerpt(ex)
    setEditNote(ex.note || '')
  }

  // 保存笔记
  const saveNote = () => {
    if (selectedExcerpt) {
      updateNote(selectedExcerpt.id, editNote)
      setSelectedExcerpt({ ...selectedExcerpt, note: editNote })
    }
  }

  // 切换状态
  const cycleStatus = () => {
    if (!selectedExcerpt) return
    const order: ReadStatus[] = ['reading', 'read', 'mastered', 'wishlist']
    const cur = order.indexOf(selectedExcerpt.status as ReadStatus)
    const next = order[(cur + 1) % order.length]
    setStatus(selectedExcerpt.id, next)
    setSelectedExcerpt({ ...selectedExcerpt, status: next })
  }

  return (
    <View className="excerpt-page">
      {/* 头部 + 统计 */}
      <View className="excerpt-header">
        <View className="header-info">
          <Text className="page-title">📚 摘抄管理</Text>
          <Text className="page-subtitle">
            共 {stats.total} 条摘抄 · {stats.books} 本书 · {stats.favorites} 收藏
          </Text>
        </View>
        <View className="header-stats">
          {stats.reading > 0 && (
            <View className="stat-mini">
              <Text className="stat-num">{stats.reading}</Text>
              <Text className="stat-label">在读</Text>
            </View>
          )}
          {stats.mastered > 0 && (
            <View className="stat-mini">
              <Text className="stat-num">{stats.mastered}</Text>
              <Text className="stat-label">掌握</Text>
            </View>
          )}
        </View>
      </View>

      {/* 搜索框 */}
      <View className="search-bar">
        <Text className="search-ic">🔍</Text>
        <Input
          className="search-input"
          placeholder="搜索摘抄内容、书名..."
          value={searchQuery}
          onInput={(e: any) => setSearchQuery(e.detail.value)}
        />
      </View>

      {/* 过滤器 */}
      <View className="filters">
        <View className="filter-group">
          <Text className="filter-label">📖 书籍</Text>
          <View className="filter-chips">
            <View
              className={`chip ${!filterBook ? 'active' : ''}`}
              onClick={() => setFilterBook('')}
            >
              <Text>全部 ({excerpts.length})</Text>
            </View>
            {books.map(book => {
              const cnt = excerpts.filter(e => e.book === book).length
              return (
                <View
                  key={book}
                  className={`chip ${filterBook === book ? 'active' : ''}`}
                  onClick={() => setFilterBook(book)}
                >
                  <Text>{book} ({cnt})</Text>
                </View>
              )
            })}
          </View>
        </View>

        <View className="filter-group">
          <Text className="filter-label">🏷️ 标签</Text>
          <View className="filter-chips">
            <View
              className={`chip ${!filterTag ? 'active' : ''}`}
              onClick={() => setFilterTag('')}
            >
              <Text>全部</Text>
            </View>
            {tags.map(tag => (
              <View
                key={tag}
                className={`chip ${filterTag === tag ? 'active' : ''}`}
                onClick={() => setFilterTag(tag)}
              >
                <Text>#{tag}</Text>
              </View>
            ))}
          </View>
        </View>

        <View className="filter-group">
          <Text className="filter-label">📅 年份</Text>
          <View className="filter-chips">
            <View
              className={`chip ${!filterYear ? 'active' : ''}`}
              onClick={() => setFilterYear('')}
            >
              <Text>全部</Text>
            </View>
            {years.map(year => (
              <View
                key={year}
                className={`chip ${filterYear === year ? 'active' : ''}`}
                onClick={() => setFilterYear(year)}
              >
                <Text>{year}</Text>
              </View>
            ))}
          </View>
        </View>
      </View>

      {/* 添加按钮 */}
      <Button className="add-btn" onClick={() => setShowAddForm(!showAddForm)}>
        {showAddForm ? '✕ 取消' : '+ 添加摘抄'}
      </Button>

      {/* 新增表单 */}
      {showAddForm && (
        <View className="add-form">
          <View className="form-item">
            <Text className="form-label">摘抄内容 *</Text>
            <Textarea
              className="form-textarea"
              placeholder="输入摘抄内容..."
              value={newExcerpt.content}
              onInput={(e: any) => setNewExcerpt({ ...newExcerpt, content: e.detail.value })}
            />
          </View>
          <View className="form-item">
            <Text className="form-label">书籍名称 *</Text>
            <Input
              className="form-input"
              placeholder="输入书籍名称"
              value={newExcerpt.book}
              onInput={(e: any) => setNewExcerpt({ ...newExcerpt, book: e.detail.value })}
            />
          </View>
          <View className="form-item">
            <Text className="form-label">标签（逗号分隔）</Text>
            <Input
              className="form-input"
              placeholder="如: 算法, 设计模式"
              value={newExcerpt.tags}
              onInput={(e: any) => setNewExcerpt({ ...newExcerpt, tags: e.detail.value })}
            />
          </View>
          <View className="form-item">
            <Text className="form-label">个人笔记</Text>
            <Textarea
              className="form-textarea note"
              placeholder="写下你的想法..."
              value={newExcerpt.note}
              onInput={(e: any) => setNewExcerpt({ ...newExcerpt, note: e.detail.value })}
            />
          </View>
          <Button className="submit-btn" onClick={handleAdd}>
            💾 保存摘抄
          </Button>
        </View>
      )}

      {/* 摘抄列表 */}
      <View className="excerpt-list">
        <View className="list-header">
          <Text className="list-count">📋 {filteredExcerpts.length} 条结果</Text>
        </View>
        {filteredExcerpts.length === 0 ? (
          <View className="empty-state">
            <Text className="empty-icon">📚</Text>
            <Text className="empty-text">暂无匹配的摘抄</Text>
            <Text className="empty-sub">尝试调整过滤器或添加新摘抄</Text>
          </View>
        ) : (
          filteredExcerpts.slice(0, 100).map(excerpt => (
            <View
              key={excerpt.id}
              className="excerpt-card"
              onClick={() => openDetail(excerpt)}
            >
              <View className="card-header">
                <View className="book-badge">
                  <Text className="book-name">{excerpt.book}</Text>
                  {excerpt.date && <Text className="book-year">· {excerpt.date}</Text>}
                </View>
                <View className="header-actions">
                  {excerpt.status && (
                    <View
                      className="status-badge"
                      style={{ background: STATUS_COLOR[excerpt.status] }}
                    >
                      <Text>{STATUS_LABEL[excerpt.status]}</Text>
                    </View>
                  )}
                  <View
                    className={`favorite-icon ${excerpt.favorite ? 'active' : ''}`}
                    onClick={(e) => { e.stopPropagation(); toggleFavorite(excerpt.id) }}
                  >
                    <Text>{excerpt.favorite ? '❤️' : '🤍'}</Text>
                  </View>
                </View>
              </View>
              <Text className="excerpt-content">{excerpt.content}</Text>
              <View className="card-footer">
                <View className="tags">
                  {excerpt.tags.map((tag, idx) => (
                    <View key={idx} className="tag">
                      <Text>#{tag}</Text>
                    </View>
                  ))}
                </View>
                <Text className="date">
                  {excerpt.source ? `📄 ${excerpt.source}` : '✏️ 手动'}
                </Text>
              </View>
              {excerpt.note && (
                <View className="note-preview">
                  <Text className="note-label">📝 笔记:</Text>
                  <Text className="note-text">{excerpt.note}</Text>
                </View>
              )}
            </View>
          ))
        )}
        {filteredExcerpts.length > 100 && (
          <View className="more-tip">
            <Text>仅显示前 100 条，请用过滤器缩小范围</Text>
          </View>
        )}
      </View>

      {/* 详情弹窗 */}
      {selectedExcerpt && (
        <View className="modal-overlay" onClick={() => setSelectedExcerpt(null)}>
          <View className="modal-content" onClick={(e) => e.stopPropagation()}>
            <View className="modal-header">
              <View className="modal-title-wrap">
                <Text className="modal-title">{selectedExcerpt.book}</Text>
                {selectedExcerpt.date && <Text className="modal-year">{selectedExcerpt.date}</Text>}
              </View>
              <Text className="close-btn" onClick={() => setSelectedExcerpt(null)}>✕</Text>
            </View>

            <Text className="modal-content-text">{selectedExcerpt.content}</Text>

            <View className="modal-status-row">
              <Text className="status-label">阅读状态：</Text>
              <View
                className="status-btn"
                style={{ background: selectedExcerpt.status ? (STATUS_COLOR as any)[selectedExcerpt.status] : 'var(--card-bg)' }}
                onClick={cycleStatus}
              >
                <Text>{selectedExcerpt.status ? (STATUS_LABEL as any)[selectedExcerpt.status] : '未设置 - 点击切换'}</Text>
              </View>
            </View>

            <View className="modal-note-edit">
              <Text className="note-title">📝 我的笔记</Text>
              <Textarea
                className="modal-note-textarea"
                value={editNote}
                onInput={(e: any) => setEditNote(e.detail.value)}
                placeholder="写下你的想法..."
              />
              <Button className="save-note-btn" onClick={saveNote}>保存笔记</Button>
            </View>

            <View className="modal-tags">
              {selectedExcerpt.tags.map((tag: string, idx: number) => (
                <View key={idx} className="tag">
                  <Text>#{tag}</Text>
                </View>
              ))}
            </View>

            <View className="modal-actions">
              <Button
                className="action-btn"
                onClick={() => handleCopy(selectedExcerpt.content)}
              >
                📋 复制摘抄
              </Button>
              <Button
                className={`action-btn ${selectedExcerpt.favorite ? 'favorited' : ''}`}
                onClick={() => {
                  toggleFavorite(selectedExcerpt.id)
                  setSelectedExcerpt({ ...selectedExcerpt, favorite: !selectedExcerpt.favorite })
                }}
              >
                {selectedExcerpt.favorite ? '❤️ 已收藏' : '🤍 收藏'}
              </Button>
              <Button
                className="action-btn danger"
                onClick={() => {
                  if (confirm('确定要删除这条摘抄吗？')) {
                    deleteExcerpt(selectedExcerpt.id)
                    setSelectedExcerpt(null)
                  }
                }}
              >
                🗑️ 删除
              </Button>
            </View>
          </View>
        </View>
      )}
    </View>
  )
}

export default Excerpt