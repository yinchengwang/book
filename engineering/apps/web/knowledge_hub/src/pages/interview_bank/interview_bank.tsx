/**
 * 面试题库页面
 *
 * 三大模式：
 * - home: 分类总览（统计 + 算法/面试切换 + 分类 grid）
 * - list: 分类题目列表（题目卡 + 难度筛选 + 搜索）
 * - detail: 详情弹窗（markdown 渲染答案）
 */
import { useState, useMemo, useEffect } from 'react'
import { View, Text, Input } from '@tarojs/components'
import {
  algoBank, interviewBank, algoCategories, interviewCategories,
  getAlgoByCategory, getInterviewBySource,
  AlgoQuestion, InterviewQuestion
} from '@/data/interview_bank'
import { useInterviewBankStore } from '@/stores/interviewBankStore'
import './interview_bank.scss'

type Mode = 'home' | 'list'
type Tab = 'algorithm' | 'interview'

const InterviewBank = () => {
  const [mode, setMode] = useState<Mode>('home')
  const [tab, setTab] = useState<Tab>('algorithm')
  const [search, setSearch] = useState('')
  const [activeCat, setActiveCat] = useState<string | null>(null)
  const [difficulty, setDifficulty] = useState<'all' | 'easy' | 'medium' | 'hard'>('all')
  const [detailItem, setDetailItem] = useState<AlgoQuestion | InterviewQuestion | null>(null)

  const { viewed, bookmarks, markViewed, toggleBookmark, isViewed, isBookmarked } = useInterviewBankStore()

  // 打开详情时标记已浏览
  useEffect(() => {
    if (detailItem) markViewed(detailItem.id)
  }, [detailItem, markViewed])

  // 当前分类的题目列表
  const currentList = useMemo(() => {
    if (!activeCat) return []
    if (tab === 'algorithm') {
      let list = getAlgoByCategory(activeCat)
      if (difficulty !== 'all') list = list.filter(q => q.difficulty === difficulty)
      if (search.trim()) {
        const q = search.toLowerCase()
        list = list.filter(item =>
          item.title.toLowerCase().includes(q) ||
          item.tags.some(t => t.toLowerCase().includes(q))
        )
      }
      return list
    } else {
      let list = getInterviewBySource(activeCat)
      if (difficulty !== 'all') list = list.filter(q => q.difficulty === difficulty)
      if (search.trim()) {
        const q = search.toLowerCase()
        list = list.filter(item => item.title.toLowerCase().includes(q))
      }
      return list
    }
  }, [tab, activeCat, search, difficulty])

  // 当前分类元数据
  const currentCatMeta = useMemo(() => {
    const cats = tab === 'algorithm' ? algoCategories : interviewCategories
    return cats.find(c => c.id === activeCat)
  }, [tab, activeCat])

  // 统计
  const algoTotal = algoBank.length
  const interviewTotal = interviewBank.length
  const viewedTotal = viewed.length
  const bookmarkTotal = bookmarks.length

  // 进入分类
  const enterCategory = (id: string) => {
    setActiveCat(id)
    setSearch('')
    setDifficulty('all')
    setMode('list')
  }

  const backToHome = () => {
    setMode('home')
    setActiveCat(null)
  }

  // 切换 tab
  const switchTab = (t: Tab) => {
    setTab(t)
    setMode('home')
    setActiveCat(null)
  }

  // 渲染题目卡 - 算法
  const renderAlgoCard = (q: AlgoQuestion) => (
    <View
      key={q.id}
      className={`q-card algo ${isViewed(q.id) ? 'viewed' : ''}`}
      onClick={() => setDetailItem(q)}
    >
      <View className="q-card-main">
        <Text className={`q-diff ${q.difficulty}`}>{diffLabel(q.difficulty)}</Text>
        <Text className="q-title">{q.title}</Text>
      </View>
      <View className="q-card-meta">
        {q.tags.slice(0, 3).map(t => <Text key={t} className="q-tag">{t}</Text>)}
        {isBookmarked(q.id) && <Text className="q-bookmark">⭐</Text>}
      </View>
    </View>
  )

  // 渲染题目卡 - 面试
  const renderInterviewCard = (q: InterviewQuestion) => (
    <View
      key={q.id}
      className={`q-card interview ${isViewed(q.id) ? 'viewed' : ''}`}
      onClick={() => setDetailItem(q)}
    >
      <View className="q-card-main">
        <Text className={`q-diff ${q.difficulty}`}>{diffLabel(q.difficulty)}</Text>
        <Text className="q-title">{q.title}</Text>
      </View>
      <View className="q-card-meta">
        <Text className="q-source">{sourceLabel(q.source)}</Text>
        {isBookmarked(q.id) && <Text className="q-bookmark">⭐</Text>}
      </View>
    </View>
  )

  // ============ 详情弹窗 ============
  const renderDetail = () => {
    if (!detailItem) return null
    const isAlgo = 'problemId' in detailItem
    const algoItem = isAlgo ? (detailItem as AlgoQuestion) : null
    const interviewItem = !isAlgo ? (detailItem as InterviewQuestion) : null

    return (
      <View className="modal-overlay" onClick={() => setDetailItem(null)}>
        <View className="modal" onClick={(e: any) => e.stopPropagation()}>
          <View className="modal-header">
            <View className="modal-title-wrap">
              <Text className={`q-diff ${detailItem.difficulty}`}>{diffLabel(detailItem.difficulty)}</Text>
              <Text className="modal-title">{detailItem.title}</Text>
            </View>
            <View
              className={`bookmark-btn ${isBookmarked(detailItem.id) ? 'active' : ''}`}
              onClick={() => toggleBookmark(detailItem.id)}
            >
              <Text>{isBookmarked(detailItem.id) ? '⭐' : '☆'}</Text>
            </View>
          </View>

          {algoItem && (
            <View className="modal-meta">
              <Text className="meta-item">📌 {algoItem.categoryTitle}</Text>
              <Text className="meta-item">🆔 #{algoItem.problemId}</Text>
              {algoItem.url && (
                <Text
                  className="meta-link"
                  onClick={() => algoItem.url && window.open(algoItem.url, '_blank')}
                >
                  🔗 LeetCode
                </Text>
              )}
            </View>
          )}

          {interviewItem && (
            <View className="modal-meta">
              <Text className="meta-item">📚 {sourceLabel(interviewItem.source)}</Text>
            </View>
          )}

          <View className="modal-body">
            {algoItem ? (
              <View className="algo-detail">
                <Text className="algo-tip">点击右上角 LeetCode 链接开始刷题 👆</Text>
                <View className="algo-tags">
                  {algoItem.tags.map(t => <Text key={t} className="q-tag">{t}</Text>)}
                </View>
                <Text className="algo-platform">来源：{algoItem.platform}</Text>
              </View>
            ) : interviewItem ? (
              <View className="markdown-content">
                {renderMarkdown(interviewItem.answer)}
              </View>
            ) : null}
          </View>

          <View className="btn-row">
            <View className="btn-close" onClick={() => setDetailItem(null)}>
              <Text>关闭</Text>
            </View>
          </View>
        </View>
      </View>
    )
  }

  // ============ HOME 视图 ============
  if (mode === 'home') {
    const cats = tab === 'algorithm' ? algoCategories : interviewCategories
    return (
      <View className="ib-page">
        <View className="ib-header">
          <Text className="ib-title">📚 面试题库</Text>
          <Text className="ib-subtitle">{algoTotal} 算法题 · {interviewTotal} 面试题 · {algoCategories.length + interviewCategories.length} 个分类</Text>
        </View>

        {/* 统计 */}
        <View className="stats-row">
          <View className="stat-box"><Text className="num">{algoTotal}</Text><Text className="label">算法题</Text></View>
          <View className="stat-box"><Text className="num">{interviewTotal}</Text><Text className="label">面试题</Text></View>
          <View className="stat-box good"><Text className="num">{viewedTotal}</Text><Text className="label">已浏览</Text></View>
          <View className="stat-box warn"><Text className="num">{bookmarkTotal}</Text><Text className="label">收藏</Text></View>
        </View>

        {/* Tab 切换 */}
        <View className="ib-tabs">
          <View
            className={`ib-tab ${tab === 'algorithm' ? 'active' : ''}`}
            onClick={() => switchTab('algorithm')}
          >
            <Text>📘 算法题库 ({algoCategories.length})</Text>
          </View>
          <View
            className={`ib-tab ${tab === 'interview' ? 'active' : ''}`}
            onClick={() => switchTab('interview')}
          >
            <Text>💼 真实面试题 ({interviewCategories.length})</Text>
          </View>
        </View>

        {/* 分类网格 */}
        <View className="cat-grid">
          {cats.map(c => (
            <View key={c.id} className="cat-card" onClick={() => enterCategory(c.id)}>
              <Text className="cat-icon">{c.icon}</Text>
              <Text className="cat-title">{c.title}</Text>
              <Text className="cat-count">{c.count} 题</Text>
              {c.desc && <Text className="cat-desc">{c.desc.slice(0, 50)}{c.desc.length > 50 ? '...' : ''}</Text>}
            </View>
          ))}
        </View>

        {renderDetail()}
      </View>
    )
  }

  // ============ LIST 视图 ============
  return (
    <View className="ib-page">
      <View className="ib-header compact">
        <View className="ib-back" onClick={backToHome}>
          <Text>← 返回</Text>
        </View>
        <Text className="ib-cat-title">{currentCatMeta?.icon} {currentCatMeta?.title}</Text>
      </View>

      {/* 搜索 + 难度 */}
      <View className="ib-toolbar">
        <Input
          className="search-input"
          placeholder="🔍 搜索题目..."
          value={search}
          onInput={(e: any) => setSearch(e.detail.value)}
        />
        <View className="diff-filter">
          {(['all', 'easy', 'medium', 'hard'] as const).map(d => (
            <View
              key={d}
              className={`diff-chip ${difficulty === d ? 'active' : ''}`}
              onClick={() => setDifficulty(d)}
            >
              <Text>{d === 'all' ? '全部' : diffLabel(d)}</Text>
            </View>
          ))}
        </View>
      </View>

      <Text className="list-info">共 {currentList.length} 题</Text>

      <View className="q-list">
        {currentList.length === 0 ? (
          <View className="empty-state">
            <Text>暂无题目</Text>
          </View>
        ) : tab === 'algorithm' ? (
          (currentList as AlgoQuestion[]).map(q => renderAlgoCard(q))
        ) : (
          (currentList as InterviewQuestion[]).map(q => renderInterviewCard(q))
        )}
      </View>

      {renderDetail()}
    </View>
  )
}

// 工具函数
function diffLabel(d: string) {
  return d === 'easy' ? '简单' : d === 'medium' ? '中等' : d === 'hard' ? '困难' : d
}

function sourceLabel(s: string) {
  const map: Record<string, string> = {
    'ccpp': 'C/C++ 基础',
    'ccpp-extra': 'C/C++ 补充',
    'database': '数据库基础',
    'database-interview': '数据库面试',
    'database-project': '数据库项目',
    'vdb': '向量数据库',
    'vdb-extra': 'VDB 专题'
  }
  return map[s] || s
}

// 简单 Markdown 渲染
function renderMarkdown(text: string): React.ReactNode[] {
  const lines = text.split('\n')
  const out: React.ReactNode[] = []
  let inCode = false
  let codeBuf: string[] = []
  let codeKey = 0

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i]
    if (line.trim().startsWith('```')) {
      if (inCode) {
        out.push(<pre key={`c${codeKey++}`} className="md-pre"><code>{codeBuf.join('\n')}</code></pre>)
        codeBuf = []
        inCode = false
      } else {
        inCode = true
      }
      continue
    }
    if (inCode) {
      codeBuf.push(line)
      continue
    }
    // 标题
    if (/^### /.test(line)) {
      out.push(<Text key={`h3${i}`} className="md-h3">{line.replace(/^### /, '')}</Text>)
    } else if (/^## /.test(line)) {
      out.push(<Text key={`h2${i}`} className="md-h2">{line.replace(/^## /, '')}</Text>)
    } else if (/^# /.test(line)) {
      out.push(<Text key={`h1${i}`} className="md-h1">{line.replace(/^# /, '')}</Text>)
    } else if (/^- /.test(line) || /^\* /.test(line)) {
      out.push(<Text key={`li${i}`} className="md-li">• {line.replace(/^[-*] /, '')}</Text>)
    } else if (line.trim()) {
      out.push(<Text key={`p${i}`} className="md-p">{renderInlineMd(line)}</Text>)
    }
  }
  return out
}

// 行内 markdown（粗体、代码）
function renderInlineMd(line: string): React.ReactNode[] {
  const out: React.ReactNode[] = []
  const parts = line.split(/(`[^`]+`|\*\*[^*]+\*\*)/g)
  parts.forEach((p, i) => {
    if (!p) return
    if (/^`[^`]+`$/.test(p)) {
      out.push(<Text key={i} className="md-code">{p.slice(1, -1)}</Text>)
    } else if (/^\*\*[^*]+\*\*$/.test(p)) {
      out.push(<Text key={i} className="md-bold">{p.slice(2, -2)}</Text>)
    } else {
      out.push(p)
    }
  })
  return out
}

export default InterviewBank
