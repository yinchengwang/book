/**
 * 雷达图页面
 *
 * 多主题切换：
 * - 书籍推荐：BOOK_DATA（4 象限 × 3 环 = 41 本经典书）
 * - 技术栈雷达：7 个 stack（c/cpp/db/ds/linux/py/vdb），每个 8 维度掌握度
 *
 * 圆点布局：角度均分 + 分层避让 + 8 轮力导向碰撞
 */
import { useState, useMemo, useEffect, useRef } from 'react'
import { View, Text, Input } from '@tarojs/components'
import {
  BOOK_DATA, RadarBook,
  QUADRANT_META, RING_META,
  ALL_QUADRANTS, ALL_RINGS,
  getQuadrantLabel, getRingLabel,
  getBooksByQuadrant
} from '@/data/radar-data'
import {
  STACK_RADARS, STACK_DIMENSIONS, ALL_STACKS,
  buildStackRadar, StackRadar, StackId
} from '@/data/radar-stack-data'
import './Radar.scss'

interface PlacedDot {
  id: string
  label: string
  x: number
  y: number
  r: number
  quad: string
  ring: string
  rec: number
  source: 'book' | 'stack'
  data: any
}

const SIZE = 560
const CX = SIZE / 2
const CY = SIZE / 2
const MAX_R = SIZE / 2 - 60

type Theme = 'book' | StackId

const Radar = () => {
  const [theme, setTheme] = useState<Theme>('book')
  const [search, setSearch] = useState('')
  const [activeQuad, setActiveQuad] = useState<string>('all')
  const [activeRing, setActiveRing] = useState<string>('all')
  const [selected, setSelected] = useState<any>(null)
  const [size, setSize] = useState(SIZE)
  const containerRef = useRef<HTMLDivElement | null>(null)

  // 响应式尺寸
  useEffect(() => {
    const update = () => {
      if (containerRef.current) {
        const w = containerRef.current.clientWidth
        setSize(Math.min(w - 32, SIZE))
      }
    }
    update()
    window.addEventListener('resize', update)
    return () => window.removeEventListener('resize', update)
  }, [])

  // 当前主题数据
  const currentData = useMemo(() => {
    if (theme === 'book') {
      return {
        type: 'book' as const,
        dots: BOOK_DATA,
        title: '📚 书籍推荐雷达',
        subtitle: `${BOOK_DATA.length} 本经典书籍 · 4 象限 × 3 环 · 圆点大小 = 推荐度`,
        quadrants: QUADRANT_META,
        rings: RING_META
      }
    } else {
      const stack = buildStackRadar(theme as StackId)
      return {
        type: 'stack' as const,
        data: stack,
        title: `${stack.icon} ${stack.name} 雷达`,
        subtitle: `8 维度掌握度 · 满分 100 · 真实值由知识图谱驱动`,
        quadrants: null,
        rings: null
      }
    }
  }, [theme])

  // 书籍主题：过滤
  const filtered = useMemo(() => {
    if (currentData.type !== 'book') return []
    return BOOK_DATA.filter(b => {
      if (activeQuad !== 'all' && b.quadrant !== activeQuad) return false
      if (activeRing !== 'all' && b.ring !== activeRing) return false
      if (search.trim()) {
        const q = search.toLowerCase()
        const match = b.title.toLowerCase().includes(q) ||
                      b.author.toLowerCase().includes(q) ||
                      b.tags.some(t => t.toLowerCase().includes(q))
        if (!match) return false
      }
      return true
    })
  }, [currentData, activeQuad, activeRing, search])

  // 缩放
  const scale = size / SIZE
  const cx = CX * scale
  const cy = CY * scale
  const maxR = MAX_R * scale

  // 圆点布局
  const dots = useMemo<PlacedDot[]>(() => {
    if (currentData.type === 'book') {
      const placed: PlacedDot[] = []
      const buckets: Record<string, RadarBook[]> = {}
      for (const b of filtered) {
        const key = `${b.quadrant}|${b.ring}`
        if (!buckets[key]) buckets[key] = []
        buckets[key].push(b)
      }
      for (const [key, books] of Object.entries(buckets)) {
        const [quad, ring] = key.split('|')
        const qMeta = QUADRANT_META[quad]
        const N = books.length
        const maxPerLayer = 4  // 减少每层避免覆盖
        const layers = Math.ceil(N / maxPerLayer)
        const rMeta = RING_META[ring]
        const ringMidR = (rMeta.min + rMeta.max) / 2
        const ringR = ringMidR * maxR

        books.forEach((book, idx) => {
          const layer = Math.floor(idx / maxPerLayer)
          const idxInLayer = idx % maxPerLayer
          const countInLayer = Math.min(maxPerLayer, N - layer * maxPerLayer)
          const angleFrac = countInLayer <= 1 ? 0.5 : idxInLayer / (countInLayer - 1)
          const margin = 10
          const angleSpan = (qMeta.angleEnd - qMeta.angleStart) - margin * 2
          const angle = qMeta.angleStart + margin + angleFrac * angleSpan

          let layerOffset = 0
          if (layers > 1) {
            const layerFrac = (layer + 0.5) / layers
            layerOffset = ((layerFrac - 0.5) * 0.7) * (rMeta.max - rMeta.min) * maxR
          }
          const radius = ringR + layerOffset

          const rad = (angle - 90) * Math.PI / 180
          const dotR = (6 + book.rec * 2.2) * scale
          placed.push({
            id: book.id,
            label: book.title,
            x: cx + Math.cos(rad) * radius,
            y: cy + Math.sin(rad) * radius,
            r: dotR,
            quad,
            ring,
            rec: book.rec,
            source: 'book',
            data: book
          })
        })
      }
      return placed
    } else {
      // Stack 雷达：8 个维度均匀分布在圆周上
      const stack = currentData.data as StackRadar
      const placed: PlacedDot[] = []
      const dims = stack.dimensions
      const N = dims.length
      const angleStep = (2 * Math.PI) / N
      // 圆点半径按 value/100 缩放（5-15）
      const baseR = maxR * 0.7  // 数据半径

      dims.forEach((dim, idx) => {
        const angle = -Math.PI / 2 + idx * angleStep
        const r = (dim.value / 100) * baseR
        placed.push({
          id: `${stack.id}-${dim.key}`,
          label: dim.label,
          x: cx + Math.cos(angle) * r,
          y: cy + Math.sin(angle) * r,
          r: (6 + dim.value / 12) * scale,
          quad: '',
          ring: '',
          rec: dim.value / 20,
          source: 'stack',
          data: dim
        })
      })
      return placed
    }
  }, [currentData, filtered, scale, cx, cy, maxR])

  // 8 轮碰撞检测
  const adjustedDots = useMemo(() => {
    const result = dots.map(d => ({ ...d }))
    const MIN_GAP = 6 * scale
    const ITERATIONS = 8
    for (let iter = 0; iter < ITERATIONS; iter++) {
      for (let i = 0; i < result.length; i++) {
        for (let j = i + 1; j < result.length; j++) {
          const a = result[i]
          const b = result[j]
          const dx = a.x - b.x
          const dy = a.y - b.y
          const dist = Math.sqrt(dx * dx + dy * dy)
          const minDist = a.r + b.r + MIN_GAP
          if (dist < minDist && dist > 0.01) {
            const decay = Math.max(0.2, 1 - iter * 0.12)
            const force = (minDist - dist) / 2 * decay
            const ux = dx / dist
            const uy = dy / dist
            a.x -= ux * force
            a.y -= uy * force
            b.x += ux * force
            b.y += uy * force
          }
        }
      }
    }
    return result
  }, [dots, scale])

  // 4 条轴线（仅书籍主题）
  const axes = useMemo(() => {
    if (currentData.type !== 'book') return []
    return ALL_QUADRANTS.map(q => {
      const meta = QUADRANT_META[q]
      const midAngle = (meta.angleStart + meta.angleEnd) / 2
      const rad = (midAngle - 90) * Math.PI / 180
      return {
        q,
        x2: cx + Math.cos(rad) * maxR,
        y2: cy + Math.sin(rad) * maxR,
        labelX: cx + Math.cos(rad) * (maxR + 24),
        labelY: cy + Math.sin(rad) * (maxR + 24),
        meta
      }
    })
  }, [currentData, cx, cy, maxR])

  // 3 层环（仅书籍主题）
  const rings = useMemo(() => {
    if (currentData.type !== 'book') return []
    return ALL_RINGS.map((r) => {
      const meta = RING_META[r]
      const rMid = (meta.min + meta.max) / 2 * maxR
      return { r, meta, rMid }
    })
  }, [currentData, maxR])

  // 象限统计
  const quadCounts = useMemo(() => {
    if (currentData.type !== 'book') return {}
    const out: Record<string, number> = {}
    for (const b of filtered) out[b.quadrant] = (out[b.quadrant] || 0) + 1
    return out
  }, [filtered, currentData])

  // Stack 雷达的"外轮廓"多边形
  const stackPolygon = useMemo(() => {
    if (currentData.type !== 'stack') return ''
    const stack = currentData.data as StackRadar
    return stack.dimensions.map((dim, idx) => {
      const angle = -Math.PI / 2 + idx * (2 * Math.PI / stack.dimensions.length)
      const r = (dim.value / 100) * maxR * 0.7
      return `${cx + Math.cos(angle) * r},${cy + Math.sin(angle) * r}`
    }).join(' ')
  }, [currentData, maxR, cx, cy])

  // Stack 雷达的"参考圆"（满分 100）
  const stackRefPolygons = useMemo(() => {
    if (currentData.type !== 'stack') return []
    return [25, 50, 75, 100].map(percent => {
      const r = (percent / 100) * maxR * 0.7
      const points: string[] = []
      const N = (currentData.data as StackRadar).dimensions.length
      for (let i = 0; i < N; i++) {
        const angle = -Math.PI / 2 + i * (2 * Math.PI / N)
        points.push(`${cx + Math.cos(angle) * r},${cy + Math.sin(angle) * r}`)
      }
      return { percent, points: points.join(' ') }
    })
  }, [currentData, maxR, cx, cy])

  return (
    <View className="radar-page">
      {/* 主题切换 Tab */}
      <View className="theme-tabs">
        <View
          className={`theme-tab ${theme === 'book' ? 'active' : ''}`}
          onClick={() => { setTheme('book'); setActiveQuad('all'); setActiveRing('all') }}
        >
          <Text>📚 书籍</Text>
        </View>
        {ALL_STACKS.map(s => (
          <View
            key={s}
            className={`theme-tab stack-tab-${s} ${theme === s ? 'active' : ''}`}
            onClick={() => setTheme(s)}
          >
            <Text>{STACK_RADARS[s].icon} {STACK_RADARS[s].name}</Text>
          </View>
        ))}
      </View>

      {/* 头部 */}
      <View className="radar-header">
        <Text className="radar-title">{currentData.title}</Text>
        <Text className="radar-subtitle">{currentData.subtitle}</Text>
      </View>

      {theme === 'book' && (
        <>
          {/* 搜索 + 筛选 */}
          <View className="radar-toolbar">
            <View className="search-wrap">
              <Text className="search-ic">🔍</Text>
              <Input
                className="search-input"
                placeholder="搜索书名/作者/标签..."
                value={search}
                onInput={(e: any) => setSearch(e.detail.value)}
              />
            </View>
          </View>

          <View className="filter-section">
            <View className="filter-row">
              <Text className="filter-label">象限：</Text>
              <View
                className={`filter-chip ${activeQuad === 'all' ? 'active' : ''}`}
                onClick={() => setActiveQuad('all')}
              >
                <Text>全部 ({BOOK_DATA.length})</Text>
              </View>
              {ALL_QUADRANTS.map(q => {
                const m = QUADRANT_META[q]
                const cnt = getBooksByQuadrant(q).length
                return (
                  <View
                    key={q}
                    className={`filter-chip ${activeQuad === q ? 'active' : ''}`}
                    style={activeQuad === q ? { borderColor: m.color, background: `${m.color}30`, color: '#fff' } : undefined}
                    onClick={() => setActiveQuad(q)}
                  >
                    <Text>{m.icon} {m.label} ({cnt})</Text>
                  </View>
                )
              })}
            </View>
            <View className="filter-row">
              <Text className="filter-label">环：</Text>
              <View
                className={`filter-chip ${activeRing === 'all' ? 'active' : ''}`}
                onClick={() => setActiveRing('all')}
              >
                <Text>全部</Text>
              </View>
              {ALL_RINGS.map(r => {
                const m = RING_META[r]
                return (
                  <View
                    key={r}
                    className={`filter-chip ${activeRing === r ? 'active' : ''}`}
                    onClick={() => setActiveRing(r)}
                  >
                    <Text>{m.label}</Text>
                  </View>
                )
              })}
            </View>
          </View>
        </>
      )}

      {theme !== 'book' && (
        <View className="filter-section">
          <View className="filter-row">
            <Text className="filter-label">📊 维度：</Text>
            {(currentData.data as StackRadar).dimensions.map(d => (
              <View key={d.key} className="filter-chip">
                <Text>{d.label} · {d.value}</Text>
              </View>
            ))}
          </View>
        </View>
      )}

      {/* SVG 雷达图 */}
      <View className="radar-wrap" ref={containerRef}>
        <svg
          width={size}
          height={size}
          viewBox={`0 0 ${size} ${size}`}
          className="radar-svg"
        >
          <defs>
            <radialGradient id="radarBg" cx="50%" cy="50%">
              <stop offset="0%" stopColor="rgba(99, 102, 241, 0.08)" />
              <stop offset="100%" stopColor="rgba(99, 102, 241, 0.0)" />
            </radialGradient>
          </defs>

          {/* 背景圆 */}
          <circle cx={cx} cy={cy} r={maxR} fill="url(#radarBg)" />
          <circle cx={cx} cy={cy} r={4 * scale} fill="#c084fc" />

          {/* 书籍主题：3 层环 + 4 条轴线 */}
          {theme === 'book' && (
            <>
              {rings.map(({ r, meta, rMid }) => (
                <g key={r}>
                  <circle
                    cx={cx}
                    cy={cy}
                    r={rMid}
                    fill="none"
                    stroke={meta.color}
                    strokeWidth={1.5}
                    strokeDasharray="2 4"
                  />
                  <text
                    x={cx + 4}
                    y={cy - rMid + 12}
                    fill="rgba(255,255,255,0.4)"
                    fontSize={10 * scale}
                    fontWeight="600"
                  >
                    {meta.label}
                  </text>
                </g>
              ))}

              {axes.map(({ q, x2, y2, labelX, labelY, meta }) => (
                <g key={q}>
                  <line
                    x1={cx} y1={cy} x2={x2} y2={y2}
                    stroke={meta.color}
                    strokeWidth={2}
                    opacity={0.4}
                  />
                  <text
                    x={labelX}
                    y={labelY}
                    fill={meta.color}
                    fontSize={13 * scale}
                    fontWeight="700"
                    textAnchor="middle"
                    dominantBaseline="middle"
                  >
                    {meta.icon} {meta.label}
                  </text>
                </g>
              ))}
            </>
          )}

          {/* Stack 主题：参考多边形 + 维度轴 */}
          {theme !== 'book' && (
            <>
              {stackRefPolygons.map(({ percent, points }) => (
                <polygon
                  key={percent}
                  points={points}
                  fill="none"
                  stroke="rgba(255,255,255,0.15)"
                  strokeWidth={1}
                  strokeDasharray="2 3"
                />
              ))}

              {/* 维度轴线 */}
              {(currentData.data as StackRadar).dimensions.map((dim, idx) => {
                const angle = -Math.PI / 2 + idx * (2 * Math.PI / (currentData.data as StackRadar).dimensions.length)
                const r = maxR * 0.7
                const x2 = cx + Math.cos(angle) * r
                const y2 = cy + Math.sin(angle) * r
                const labelR = r + 16
                const lx = cx + Math.cos(angle) * labelR
                const ly = cy + Math.sin(angle) * labelR
                return (
                  <g key={dim.key}>
                    <line x1={cx} y1={cy} x2={x2} y2={y2} stroke="rgba(255,255,255,0.1)" />
                    <text
                      x={lx}
                      y={ly}
                      fill="rgba(255,255,255,0.7)"
                      fontSize={10 * scale}
                      fontWeight="700"
                      textAnchor="middle"
                      dominantBaseline="middle"
                    >
                      {dim.label}
                    </text>
                  </g>
                )
              })}

              {/* 数据多边形 */}
              <polygon
                points={stackPolygon}
                fill={(currentData.data as StackRadar).color}
                fillOpacity={0.25}
                stroke={(currentData.data as StackRadar).color}
                strokeWidth={2}
              />
            </>
          )}

          {/* 数据圆点 */}
          {adjustedDots.map(d => {
            const color = d.source === 'book'
              ? QUADRANT_META[d.quad as keyof typeof QUADRANT_META]?.color
              : (currentData.data as StackRadar).color
            return (
              <g key={d.id}>
                <circle
                  cx={d.x}
                  cy={d.y}
                  r={d.r}
                  fill={color}
                  fillOpacity={d.source === 'stack' ? 0.9 : 0.5}
                  stroke={color}
                  strokeWidth={1.5}
                  className="radar-dot"
                  onClick={() => setSelected(d.data)}
                >
                  <title>{d.label} - {d.rec}/5</title>
                </circle>
                {d.r > 9 * scale && d.source === 'book' && (
                  <text
                    x={d.x}
                    y={d.y + d.r + 10 * scale}
                    fill="#fff"
                    fontSize={9 * scale}
                    textAnchor="middle"
                    className="dot-label"
                  >
                    {d.label.length > 8 ? d.label.slice(0, 8) + '…' : d.label}
                  </text>
                )}
              </g>
            )
          })}
        </svg>
      </View>

      {/* 图例 */}
      <View className="legend-row">
        {theme === 'book' ? (
          <>
            <Text className="legend-item">💡 圆点越大 = 推荐度越高（1-5）</Text>
            <Text className="legend-item">点击圆点查看详情</Text>
          </>
        ) : (
          <>
            <Text className="legend-item">
              💡 {(currentData.data as StackRadar).icon} {(currentData.data as StackRadar).name} · {(currentData.data as StackRadar).totalQuestions} 题
            </Text>
            <Text className="legend-item">圆点大小 = 掌握度</Text>
          </>
        )}
      </View>

      {/* 书籍主题：象限统计 */}
      {theme === 'book' && (
        <View className="quad-stats">
          {ALL_QUADRANTS.map(q => {
            const m = QUADRANT_META[q]
            const cnt = quadCounts[q] || 0
            const total = getBooksByQuadrant(q).length
            return (
              <View
                key={q}
                className="quad-stat"
                style={{ borderColor: m.color }}
                onClick={() => setActiveQuad(q)}
              >
                <Text className="quad-icon">{m.icon}</Text>
                <Text className="quad-name">{m.label}</Text>
                <Text className="quad-count">{cnt}/{total}</Text>
              </View>
            )
          })}
        </View>
      )}

      {/* Stack 主题：维度详细列表 */}
      {theme !== 'book' && (
        <View className="quad-stats">
          {(currentData.data as StackRadar).dimensions.map(dim => (
            <View
              key={dim.key}
              className="quad-stat"
              style={{ borderColor: (currentData.data as StackRadar).color }}
            >
              <Text className="quad-name">{dim.label}</Text>
              <Text className="quad-count">{dim.value}/100</Text>
            </View>
          ))}
        </View>
      )}

      {/* 详情弹窗 */}
      {selected && (
        <View className="modal-overlay" onClick={() => setSelected(null)}>
          <View className="modal" onClick={(e: any) => e.stopPropagation()}>
            {selected.id && BOOK_DATA.find(b => b.id === selected.id) ? (
              // 书籍详情
              (() => {
                const book = BOOK_DATA.find(b => b.id === selected.id)!
                return (
                  <>
                    <View className="modal-header">
                      <View className="modal-title-wrap">
                        <Text className="rec-badge-large rec-{book.rec}">{'★'.repeat(book.rec)}</Text>
                        <View>
                          <Text className="modal-title">{book.title}</Text>
                          <Text className="modal-author">{book.author}</Text>
                        </View>
                      </View>
                    </View>
                    <View className="modal-meta">
                      <Text
                        className="meta-item"
                        style={{ background: `${QUADRANT_META[book.quadrant].color}30`, color: QUADRANT_META[book.quadrant].color }}
                      >
                        {QUADRANT_META[book.quadrant].icon} {getQuadrantLabel(book.quadrant)}
                      </Text>
                      <Text className="meta-item">{getRingLabel(book.ring)} 环</Text>
                      <Text className="meta-item">推荐度 {book.rec}/5</Text>
                    </View>
                    <View className="modal-body">
                      <Text className="book-desc">{book.desc}</Text>
                      <View className="book-tag-list">
                        {book.tags.map(t => (
                          <Text key={t} className="book-tag-pill">#{t}</Text>
                        ))}
                      </View>
                    </View>
                  </>
                )
              })()
            ) : (
              // Stack 维度详情
              (() => {
                const dim = (currentData.data as StackRadar).dimensions.find(d => d.key === selected.key)
                if (!dim) return null
                return (
                  <>
                    <View className="modal-header">
                      <View className="modal-title-wrap">
                        <Text className="rec-badge-large">{dim.value}</Text>
                        <View>
                          <Text className="modal-title">{dim.label}</Text>
                          <Text className="modal-author">{(currentData.data as StackRadar).name}</Text>
                        </View>
                      </View>
                    </View>
                    <View className="modal-meta">
                      <Text className="meta-item">满分 {dim.max}</Text>
                      <Text className="meta-item">掌握度 {Math.round(dim.value / dim.max * 100)}%</Text>
                    </View>
                    <View className="modal-body">
                      <Text className="book-desc">
                        {dim.value >= 80 ? '🏆 已精通，建议巩固或尝试更高级主题。' :
                         dim.value >= 60 ? '✅ 已掌握，可用于实际项目。' :
                         dim.value >= 40 ? '📚 部分掌握，建议继续学习。' :
                         '🌱 入门阶段，建议从基础开始。'}
                      </Text>
                    </View>
                  </>
                )
              })()
            )}
            <View className="btn-row">
              <View className="btn-close" onClick={() => setSelected(null)}>
                <Text>关闭</Text>
              </View>
            </View>
          </View>
        </View>
      )}
    </View>
  )
}

export default Radar