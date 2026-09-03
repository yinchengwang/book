/**
 * 知识图谱页面
 *
 * - SVG 树状布局（按 domain 分层 + 按 prerequisite 关系连边）
 * - 节点可点击 → 弹详情 modal（显示掌握度、关联节点、相关题库）
 * - 边线：从源点指向目标点的 SVG line，带箭头
 * - 5 大领域颜色编码（图例 + 节点填色）
 */
import { useState, useMemo, useEffect } from 'react'
import { View, Text } from '@tarojs/components'
import { useKnowledgeStore } from '@/stores/knowledgeStore'
import { useQuizStore } from '@/stores/quizStore'
import './knowledge_graph.scss'

const DOMAIN_COLORS: Record<string, string> = {
  'C/C++': '#ef4444',
  '算法': '#8b5cf6',
  '系统设计': '#10b981',
  '数据库': '#f59e0b',
  '计算机网络': '#3b82f6'
}

const DOMAIN_ICONS: Record<string, string> = {
  'C/C++': '💻',
  '算法': '🧮',
  '系统设计': '⚙️',
  '数据库': '🗄️',
  '计算机网络': '🌐'
}

interface NodePosition {
  id: string
  x: number
  y: number
  node: any
}

const KnowledgeGraph = () => {
  const { knowledgeGraph, updateNodeLevel } = useKnowledgeStore()
  const answers = useQuizStore(s => s.answers)
  const [selectedId, setSelectedId] = useState<string | null>(null)
  const [size, setSize] = useState({ w: 720, h: 480 })

  // 响应式宽度
  useEffect(() => {
    const update = () => {
      const w = Math.min(window.innerWidth - 32, 720)
      const h = Math.min(window.innerHeight - 200, 540)
      setSize({ w, h })
    }
    update()
    window.addEventListener('resize', update)
    return () => window.removeEventListener('resize', update)
  }, [])

  // 按 domain 分组
  const groups = useMemo(() => {
    const g: Record<string, any[]> = {}
    for (const n of knowledgeGraph.nodes) {
      if (!g[n.domain]) g[n.domain] = []
      g[n.domain].push(n)
    }
    return g
  }, [knowledgeGraph.nodes])

  // 树状布局：domain 沿 x 轴排列，节点沿 y 轴均匀分布
  const positions = useMemo<NodePosition[]>(() => {
    const result: NodePosition[] = []
    const domainList = Object.keys(groups)
    const margin = 60
    const usableW = size.w - margin * 2
    const usableH = size.h - margin * 2
    const colW = domainList.length > 0 ? usableW / domainList.length : 0

    domainList.forEach((domain, ci) => {
      const nodes = groups[domain]
      const cx = margin + colW * (ci + 0.5)
      const rowH = nodes.length > 0 ? usableH / nodes.length : 0
      nodes.forEach((node, ri) => {
        const cy = margin + rowH * (ri + 0.5)
        result.push({ id: node.id, x: cx, y: cy, node })
      })
    })
    return result
  }, [groups, size])

  // 位置查询 map
  const posMap = useMemo(() => {
    const m: Record<string, NodePosition> = {}
    for (const p of positions) m[p.id] = p
    return m
  }, [positions])

  // 选中节点
  const selectedNode = useMemo(() => {
    if (!selectedId) return null
    return knowledgeGraph.nodes.find(n => n.id === selectedId) || null
  }, [selectedId, knowledgeGraph.nodes])

  // 选中节点的相关链接
  const relatedLinks = useMemo(() => {
    if (!selectedId) return []
    return knowledgeGraph.links.filter(l => l.source === selectedId || l.target === selectedId)
  }, [selectedId, knowledgeGraph.links])

  // 调整节点掌握度
  const adjustLevel = (delta: number) => {
    if (!selectedNode) return
    const newLevel = Math.max(1, Math.min(5, selectedNode.level + delta))
    updateNodeLevel(selectedNode.id, newLevel)
  }

  return (
    <View className="kg-page">
      <View className="kg-header">
        <Text className="kg-title">🔗 知识图谱</Text>
        <Text className="kg-subtitle">
          可视化知识点之间的关系 · 点击节点查看详情
        </Text>
      </View>

      {knowledgeGraph.nodes.length === 0 ? (
        <View className="kg-empty">
          <Text className="empty-icon">🕸️</Text>
          <Text className="empty-text">暂无知识图谱数据</Text>
          <Text className="empty-sub">开始答题后会自动构建你的知识网络</Text>
        </View>
      ) : (
        <>
          <View className="kg-container">
            <svg
              width={size.w}
              height={size.h}
              viewBox={`0 0 ${size.w} ${size.h}`}
              className="kg-svg"
            >
              <defs>
                <marker
                  id="arrow"
                  viewBox="0 0 10 10"
                  refX="9"
                  refY="5"
                  markerWidth="6"
                  markerHeight="6"
                  orient="auto-start-reverse"
                >
                  <path d="M 0 0 L 10 5 L 0 10 z" fill="rgba(255,255,255,0.4)" />
                </marker>
              </defs>

              {/* 背景列分组 */}
              {Object.keys(groups).map((domain, ci) => {
                const colW = (size.w - 120) / Object.keys(groups).length
                const x = 60 + colW * (ci + 0.5)
                return (
                  <g key={domain}>
                    <line
                      x1={x} y1={20}
                      x2={x} y2={size.h - 20}
                      stroke={DOMAIN_COLORS[domain] || '#6366f1'}
                      strokeOpacity={0.15}
                      strokeWidth={1}
                      strokeDasharray="2 4"
                    />
                    <text
                      x={x}
                      y={14}
                      fill={DOMAIN_COLORS[domain] || '#6366f1'}
                      fontSize={11}
                      fontWeight="700"
                      textAnchor="middle"
                    >
                      {DOMAIN_ICONS[domain] || '📌'} {domain}
                    </text>
                  </g>
                )
              })}

              {/* 连线 */}
              {knowledgeGraph.links.map((link, idx) => {
                const src = posMap[link.source]
                const tgt = posMap[link.target]
                if (!src || !tgt) return null
                const isHighlighted = selectedId && (link.source === selectedId || link.target === selectedId)
                return (
                  <line
                    key={`link-${idx}`}
                    x1={src.x} y1={src.y}
                    x2={tgt.x} y2={tgt.y}
                    stroke={isHighlighted ? '#a5b4fc' : 'rgba(255,255,255,0.2)'}
                    strokeWidth={isHighlighted ? 2 : 1}
                    markerEnd="url(#arrow)"
                  />
                )
              })}

              {/* 节点 */}
              {positions.map(p => {
                const color = DOMAIN_COLORS[p.node.domain] || '#6366f1'
                const radius = 8 + p.node.level * 2
                const isSelected = selectedId === p.id
                const isHighlighted = selectedId && relatedLinks.some(l =>
                  l.source === p.id || l.target === p.id
                )
                return (
                  <g
                    key={p.id}
                    className="kg-node"
                    onClick={() => setSelectedId(p.id)}
                  >
                    <circle
                      cx={p.x}
                      cy={p.y}
                      r={radius}
                      fill={color}
                      fillOpacity={isSelected ? 0.95 : isHighlighted ? 0.7 : 0.5}
                      stroke={isSelected ? '#fff' : color}
                      strokeWidth={isSelected ? 3 : 1.5}
                      style={{ cursor: 'pointer', transition: 'all 0.15s' }}
                    />
                    <text
                      x={p.x}
                      y={p.y + radius + 12}
                      fill="rgba(255,255,255,0.85)"
                      fontSize={10}
                      fontWeight="600"
                      textAnchor="middle"
                      style={{ pointerEvents: 'none', userSelect: 'none' }}
                    >
                      {p.node.name}
                    </text>
                    <text
                      x={p.x}
                      y={p.y + 2}
                      fill="#fff"
                      fontSize={10}
                      fontWeight="700"
                      textAnchor="middle"
                      style={{ pointerEvents: 'none' }}
                    >
                      L{p.node.level}
                    </text>
                  </g>
                )
              })}
            </svg>
          </View>

          {/* 图例 */}
          <View className="kg-legend">
            {Object.keys(groups).map(domain => (
              <View key={domain} className="legend-item">
                <View
                  className="legend-dot"
                  style={{ backgroundColor: DOMAIN_COLORS[domain] || '#6366f1' }}
                />
                <Text className="legend-text">{DOMAIN_ICONS[domain] || '📌'} {domain} ({groups[domain].length})</Text>
              </View>
            ))}
          </View>

          {/* 详情弹窗 */}
          {selectedNode && (
            <View className="modal-overlay" onClick={() => setSelectedId(null)}>
              <View className="modal" onClick={(e) => e.stopPropagation()}>
                <View className="modal-header">
                  <View
                    className="modal-icon"
                    style={{ background: DOMAIN_COLORS[selectedNode.domain] }}
                  >
                    <Text>{DOMAIN_ICONS[selectedNode.domain] || '📌'}</Text>
                  </View>
                  <View className="modal-title-wrap">
                    <Text className="modal-title">{selectedNode.name}</Text>
                    <Text className="modal-domain">{selectedNode.domain}</Text>
                  </View>
                  <Text className="modal-close" onClick={() => setSelectedId(null)}>✕</Text>
                </View>

                <View className="modal-body">
                  <View className="level-control">
                    <Text className="level-label">掌握度：</Text>
                    <View className="level-bar">
                      {[1, 2, 3, 4, 5].map(l => (
                        <View
                          key={l}
                          className={`level-dot ${l <= selectedNode.level ? 'active' : ''}`}
                          onClick={() => updateNodeLevel(selectedNode.id, l)}
                        />
                      ))}
                    </View>
                    <View className="level-buttons">
                      <Text className="level-btn" onClick={() => adjustLevel(-1)}>−</Text>
                      <Text className="level-value">L{selectedNode.level}</Text>
                      <Text className="level-btn" onClick={() => adjustLevel(1)}>+</Text>
                    </View>
                  </View>

                  {relatedLinks.length > 0 && (
                    <View className="related-section">
                      <Text className="related-title">🔗 关联节点 ({relatedLinks.length})</Text>
                      <View className="related-list">
                        {relatedLinks.map((l, idx) => {
                          const otherId = l.source === selectedNode.id ? l.target : l.source
                          const other = knowledgeGraph.nodes.find(n => n.id === otherId)
                          if (!other) return null
                          const isOutgoing = l.source === selectedNode.id
                          return (
                            <View
                              key={idx}
                              className="related-item"
                              onClick={() => setSelectedId(other.id)}
                            >
                              <Text className="related-arrow">{isOutgoing ? '→' : '←'}</Text>
                              <View
                                className="related-dot"
                                style={{ background: DOMAIN_COLORS[other.domain] }}
                              />
                              <Text className="related-name">{other.name}</Text>
                              <Text className="related-type">{isOutgoing ? '前置' : '后续'}</Text>
                            </View>
                          )
                        })}
                      </View>
                    </View>
                  )}

                  <View className="stats-section">
                    <Text className="stats-title">📊 学习数据</Text>
                    <View className="stats-row">
                      <View className="stat">
                        <Text className="stat-num">{answers.length}</Text>
                        <Text className="stat-label">总答题数</Text>
                      </View>
                      <View className="stat">
                        <Text className="stat-num">{selectedNode.level * 20}</Text>
                        <Text className="stat-label">掌握度 %</Text>
                      </View>
                    </View>
                  </View>
                </View>
              </View>
            </View>
          )}
        </>
      )}
    </View>
  )
}

export default KnowledgeGraph