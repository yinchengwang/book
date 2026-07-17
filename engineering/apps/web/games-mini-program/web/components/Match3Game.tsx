/**
 * 开心消消乐 - 完整版组件
 * 参考：开心消消乐
 *
 * 完整功能：
 * - 9x9 棋盘
 * - 章节区域系统（普通关/困难关/超难关）
 * - 章节解锁（星星解锁下一章节）
 * - 特效系统（4连→直线/5连→彩虹）
 * - 障碍物系统（雪块/蜂蜜/藤蔓/石头）
 * - 特殊元素（钥匙/宝箱/松鼠/发条鸟/蜗牛/金币）
 * - 关卡目标收集系统
 * - 星星评价系统
 * - 步数验证
 */
import React, { useState, useCallback, useRef, useEffect } from 'react'
import {
  BOARD_SIZE,
  GEM_EMOJIS,
  GEM_COLORS,
  SPECIAL_EMOJIS,
  OBSTACLE_INFO,
  ELEMENT_INFO,
  LEVEL_TYPE_INFO,
  CHAPTERS,
  Cell,
  GemType,
  SpecialType,
  ObstacleType,
  SpecialElementType,
  LevelType,
  createGameState,
  getLevelConfig,
  findMatches,
  analyzeMatchShape,
  triggerSpecial,
  dropGems,
  isValidSwap,
  doSwap,
  calculateScore,
  calculateStars,
  copyBoard,
  updateSnails,
  checkGoalsComplete,
  processElementEffect,
  GameState,
} from '../utils/match3'

interface Match3GameProps {
  onBack: () => void
}

export default function Match3Game({ onBack }: Match3GameProps) {
  // 视图模式: 'game' | 'chapter' | 'level-select'
  const [view, setView] = useState<'game' | 'chapter'>('chapter')
  const [gameState, setGameState] = useState<GameState | null>(null)
  const [selected, setSelected] = useState<string | null>(null)
  const [animating, setAnimating] = useState(false)
  const [removedGems, setRemovedGems] = useState<Set<string>>(new Set())
  const [message, setMessage] = useState('')
  const [combo, setCombo] = useState(0)

  // 已解锁的章节
  const [unlockedChapters, setUnlockedChapters] = useState<Set<number>>(new Set([1]))
  // 已获得的星星
  const [earnedStars, setEarnedStars] = useState<Record<number, number>>({})

  // 计算总星星数
  const totalStars = Object.values(earnedStars).reduce((a, b) => a + b, 0)

  // 开始关卡
  const startLevel = useCallback((level: number) => {
    setGameState(createGameState(level, earnedStars, unlockedChapters))
    setSelected(null)
    setAnimating(false)
    setRemovedGems(new Set())
    setMessage('')
    setCombo(0)
    setView('game')
  }, [earnedStars, unlockedChapters])

  // 处理格子点击
  const handleCellClick = useCallback((pos: string) => {
    if (!gameState || animating || gameState.status !== 'playing') return

    if (!selected) {
      setSelected(pos)
      return
    }

    if (selected === pos) {
      setSelected(null)
      return
    }

    // 检查是否是相邻格子
    const [r1, c1] = selected.split(',').map(Number)
    const [r2, c2] = pos.split(',').map(Number)

    if (Math.abs(r1 - r2) + Math.abs(c1 - c2) !== 1) {
      setSelected(pos)
      return
    }

    // 尝试交换
    if (isValidSwap(gameState.board, selected, pos)) {
      setAnimating(true)
      setSelected(null)
      setCombo(0)

      const newBoard = copyBoard(gameState.board)
      doSwap(newBoard, selected, pos)

      // 处理匹配
      processMatch(newBoard, gameState.level)
    } else {
      setMessage('无法移动！')
      setTimeout(() => setMessage(''), 500)
      setSelected(null)
    }
  }, [selected, gameState, animating])

  // 处理匹配逻辑
  const processMatch = useCallback((board: Cell[][], level: number) => {
    const matches = findMatches(board)
    const config = getLevelConfig(level)

    if (matches.length === 0) {
      // 没有匹配，结束这步
      setAnimating(false)

      // 更新蜗牛
      if (gameState) {
        const { snailRemoved, gameOver } = updateSnails(gameState)
        if (snailRemoved.length > 0) {
          // 有蜗牛掉落
          if (gameOver) {
            setGameState(prev => prev ? { ...prev, status: 'lost', snails: [] } : null)
          }
        }
      }

      // 检查步数
      setGameState(prev => {
        if (!prev) return null
        const newMoves = prev.moves - 1

        // 检查胜利条件
        const won = checkGoalsComplete(prev)
        const lost = newMoves <= 0 && !won

        if (won || lost) {
          // 计算星星
          const stars = won ? calculateStars(prev.score, config.targetScore, newMoves, prev.maxMoves) : 0

          // 更新记录
          if (won) {
            const newEarnedStars = { ...earnedStars, [level]: Math.max(earnedStars[level] || 0, stars) }
            setEarnedStars(newEarnedStars)

            // 检查章节解锁
            const chapter = CHAPTERS.find(c => level >= c.levels[0] && level <= c.levels[1])
            if (chapter) {
              const nextChapter = CHAPTERS.find(c => c.id === chapter.id + 1)
              if (nextChapter) {
                const newTotalStars = Object.values(newEarnedStars).reduce((a, b) => a + b, 0)
                if (newTotalStars >= nextChapter.starsRequired) {
                  setUnlockedChapters(prev => new Set([...prev, nextChapter.id]))
                }
              }
            }
          }

          return { ...prev, status: won ? 'won' : 'lost', moves: newMoves, stars }
        }

        return { ...prev, moves: newMoves }
      })
      return
    }

    setCombo(prev => prev + 1)

    // 分析匹配形状
    const shape = analyzeMatchShape(matches)
    let allRemoved = new Set<string>()

    // 处理每个匹配
    for (const match of matches) {
      const positions = Array.from(match.cells)

      // 检查是否产生特效
      let specialGem: SpecialType = null

      if (positions.length === 4) {
        // 4连：根据形状决定特效
        if (shape.intersections.some(i => positions.includes(i))) {
          specialGem = 'bomb' // L/T形产生爆炸
        } else {
          specialGem = Math.random() > 0.5 ? 'line_h' : 'line_v' // 随机横竖线
        }
      } else if (positions.length >= 5) {
        specialGem = 'rainbow' // 5连产生彩虹
      }

      // 触发特效
      for (const pos of positions) {
        const [r, c] = pos.split(',').map(Number)
        const cell = board[r][c]

        // 处理特效宝石
        if (cell.special) {
          const specialRemoved = triggerSpecial(board, pos, cell.special)
          specialRemoved.forEach(p => allRemoved.add(p))
        } else {
          allRemoved.add(pos)
        }

        // 处理特殊元素效果
        if (cell.element) {
          const elementRemoved = processElementEffect(board, pos, cell.element)
          elementRemoved.forEach(p => allRemoved.add(p))
        }
      }

      // 保留特效宝石位置
      if (specialGem) {
        const centerIdx = Math.floor(positions.length / 2)
        const centerPos = positions[centerIdx]
        const [r, c] = centerPos.split(',').map(Number)
        board[r][c].special = specialGem
        board[r][c].gem = cell.gem
      }
    }

    // 显示消除动画
    setRemovedGems(allRemoved)
    const scoreGain = calculateScore(allRemoved.size, combo + 1)
    setMessage(`${allRemoved.size}连消！+${scoreGain}`)

    setTimeout(() => {
      // 移除宝石
      allRemoved.forEach(pos => {
        const [r, c] = pos.split(',').map(Number)
        board[r][c].gem = null
      })

      // 收集特殊元素和更新目标
      const collected: Record<string, number> = {}
      const goalUpdates: Record<string, number> = {}

      for (const pos of allRemoved) {
        const [r, c] = pos.split(',').map(Number)
        const element = board[r][c].element
        if (element) {
          collected[element] = (collected[element] || 0) + 1
          goalUpdates[element] = (goalUpdates[element] || 0) + 1
        }

        // 障碍物处理
        if (board[r][c].obstacle) {
          board[r][c].obstacleHp--
          if (board[r][c].obstacleHp <= 0) {
            board[r][c].obstacle = null
            goalUpdates.score = (goalUpdates.score || 0) + 50
          }
        }

        // 清除特殊元素
        board[r][c].element = null
      }

      // 更新分数
      goalUpdates.score = (goalUpdates.score || 0) + scoreGain

      // 宝石下落
      dropGems(board)

      // 更新游戏状态
      setGameState(prev => {
        if (!prev) return null

        const newCollected = { ...prev.collected }
        Object.entries(collected).forEach(([key, val]) => {
          newCollected[key] = (newCollected[key] || 0) + val
        })

        const newGoals = prev.goals.map(g => ({
          ...g,
          current: g.current + (goalUpdates[g.type] || 0)
        }))

        return {
          ...prev,
          board,
          score: prev.score + scoreGain,
          collected: newCollected,
          goals: newGoals,
        }
      })

      setRemovedGems(new Set())

      // 检查连消
      setTimeout(() => {
        processMatch(board, level)
      }, 200)
    }, 200)
  }, [combo, gameState])

  // 重新开始
  const handleRestart = useCallback(() => {
    if (gameState) {
      setGameState(createGameState(gameState.level, earnedStars, unlockedChapters))
      setSelected(null)
      setAnimating(false)
      setRemovedGems(new Set())
      setMessage('')
      setCombo(0)
    }
  }, [gameState, earnedStars, unlockedChapters])

  // 下一关
  const handleNextLevel = useCallback(() => {
    if (gameState) {
      startLevel(gameState.level + 1)
    }
  }, [gameState, startLevel])

  // 返回章节选择
  const handleBackToChapter = useCallback(() => {
    setView('chapter')
    setGameState(null)
  }, [])

  // 格子尺寸
  const cellSize = Math.min(
    Math.floor((typeof window !== 'undefined' ? window.innerWidth : 375) * 0.9 / BOARD_SIZE),
    42
  )

  // 渲染章节选择视图
  const renderChapterView = () => {
    const config = getLevelConfig(1)

    return (
      <div style={{
        minHeight: '100vh',
        background: 'linear-gradient(180deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%)',
        color: '#fff',
        fontFamily: '-apple-system, BlinkMacSystemFont, sans-serif',
        padding: '16px',
      }}>
        {/* 顶部栏 */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: '20px',
        }}>
          <button onClick={onBack} style={{
            width: '44px', height: '44px',
            borderRadius: '50%',
            background: 'rgba(255,255,255,0.2)',
            border: 'none', fontSize: '20px',
            cursor: 'pointer',
          }}>←</button>

          <div style={{ textAlign: 'center' }}>
            <div style={{ fontSize: '24px', fontWeight: 'bold' }}>开心消消乐</div>
            <div style={{ fontSize: '14px', color: '#FFD700' }}>
              ⭐ {totalStars} 星
            </div>
          </div>

          <div style={{ width: '44px' }} />
        </div>

        {/* 章节列表 */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
          {CHAPTERS.map(chapter => {
            const isUnlocked = unlockedChapters.has(chapter.id) || totalStars >= chapter.starsRequired || chapter.id === 1
            const chapterStars = Array.from({ length: chapter.levels[1] - chapter.levels[0] + 1 }, (_, i) => {
              const lv = chapter.levels[0] + i
              return earnedStars[lv] || 0
            }).reduce((a, b) => a + b, 0)
            const maxStars = (chapter.levels[1] - chapter.levels[0] + 1) * 3

            return (
              <div
                key={chapter.id}
                onClick={() => isUnlocked && startLevel(chapter.levels[0])}
                style={{
                  background: isUnlocked
                    ? `linear-gradient(135deg, ${LEVEL_TYPE_INFO[chapter.type].bgColor}dd, ${LEVEL_TYPE_INFO[chapter.type].bgColor}88)`
                    : 'rgba(100,100,100,0.5)',
                  borderRadius: '16px',
                  padding: '16px',
                  cursor: isUnlocked ? 'pointer' : 'not-allowed',
                  opacity: isUnlocked ? 1 : 0.6,
                  border: `2px solid ${LEVEL_TYPE_INFO[chapter.type].color}`,
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                  <div>
                    <div style={{ fontSize: '18px', fontWeight: 'bold' }}>
                      第{chapter.id}章 {chapter.name}
                    </div>
                    <div style={{ fontSize: '12px', color: '#ccc' }}>
                      {LEVEL_TYPE_INFO[chapter.type].name} · {chapter.description}
                    </div>
                  </div>
                  {!isUnlocked && (
                    <div style={{ fontSize: '24px' }}>🔒</div>
                  )}
                </div>

                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div style={{ fontSize: '14px' }}>
                    关卡 {chapter.levels[0]}-{chapter.levels[1]}
                  </div>
                  <div style={{ fontSize: '16px', color: '#FFD700' }}>
                    ⭐ {chapterStars}/{maxStars}
                  </div>
                </div>

                {/* 星星进度条 */}
                <div style={{
                  height: '6px',
                  background: 'rgba(255,255,255,0.2)',
                  borderRadius: '3px',
                  marginTop: '8px',
                  overflow: 'hidden'
                }}>
                  <div style={{
                    width: `${(chapterStars / maxStars) * 100}%`,
                    height: '100%',
                    background: 'linear-gradient(90deg, #FFD700, #FFA500)',
                    borderRadius: '3px',
                  }} />
                </div>
              </div>
            )
          })}
        </div>

        {/* 解锁提示 */}
        {(() => {
          const nextLocked = CHAPTERS.find(c => !unlockedChapters.has(c.id) && totalStars < c.starsRequired && c.id !== 1)
          if (nextLocked) {
            return (
              <div style={{
                marginTop: '20px',
                textAlign: 'center',
                fontSize: '14px',
                color: '#aaa',
              }}>
                再获得 {nextLocked.starsRequired - totalStars} 星解锁：{nextLocked.name}
              </div>
            )
          }
          return null
        })()}
      </div>
    )
  }

  // 渲染游戏视图
  const renderGameView = () => {
    if (!gameState) return null

    const config = getLevelConfig(gameState.level)
    const levelColor = LEVEL_TYPE_INFO[config.type]

    // 进度条
    const scoreProgress = Math.min(100, (gameState.score / config.targetScore) * 100)

    return (
      <div style={{
        minHeight: '100vh',
        background: `linear-gradient(180deg, ${levelColor.bgColor} 0%, ${levelColor.bgColor}dd 100%)`,
        color: '#333',
        fontFamily: '-apple-system, BlinkMacSystemFont, sans-serif',
        padding: '12px',
        boxSizing: 'border-box',
      }}>
        {/* 顶部栏 */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: '10px'
        }}>
          <button onClick={handleBackToChapter} style={{
            width: '40px', height: '40px',
            borderRadius: '50%',
            background: 'rgba(255,255,255,0.9)',
            border: 'none', fontSize: '18px',
            cursor: 'pointer',
          }}>←</button>

          <div style={{
            background: `linear-gradient(135deg, ${levelColor.color} 0%, ${levelColor.color}cc 100%)`,
            padding: '8px 20px',
            borderRadius: '20px',
            fontSize: '16px',
            fontWeight: 'bold',
            color: '#fff',
          }}>
            {levelColor.name} {gameState.level}
          </div>

          <button onClick={handleRestart} style={{
            width: '40px', height: '40px',
            borderRadius: '50%',
            background: 'rgba(255,255,255,0.9)',
            border: 'none', fontSize: '18px',
            cursor: 'pointer',
          }}>🔄</button>
        </div>

        {/* 状态栏 */}
        <div style={{
          background: 'rgba(255,255,255,0.95)',
          borderRadius: '12px',
          padding: '12px 16px',
          marginBottom: '10px',
          boxShadow: '0 2px 8px rgba(0,0,0,0.1)'
        }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '8px',
            fontSize: '14px'
          }}>
            <span>🎯 目标: <strong>{config.targetScore}</strong></span>
            <span style={{ color: '#FF6B6B' }}>⭐ {gameState.score}</span>
            <span style={{ color: gameState.moves <= 5 ? '#FF6B6B' : '#333' }}>
              👣 {gameState.moves}
            </span>
          </div>

          {/* 进度条 */}
          <div style={{
            height: '8px',
            background: '#E8E8E8',
            borderRadius: '4px',
            overflow: 'hidden'
          }}>
            <div style={{
              width: `${scoreProgress}%`,
              height: '100%',
              background: scoreProgress >= 100
                ? 'linear-gradient(90deg, #26DE81, #20BF6B)'
                : 'linear-gradient(90deg, #FFD700, #FFA500)',
              borderRadius: '4px',
              transition: 'width 0.3s ease'
            }} />
          </div>

          {/* 目标进度 */}
          <div style={{
            display: 'flex',
            flexWrap: 'wrap',
            gap: '8px',
            marginTop: '8px',
            fontSize: '12px'
          }}>
            {gameState.goals.map(goal => (
              <div key={goal.type} style={{
                background: goal.current >= goal.target ? '#26DE81' : '#f0f0f0',
                padding: '4px 8px',
                borderRadius: '8px',
                color: goal.current >= goal.target ? '#fff' : '#333',
              }}>
                {ELEMENT_INFO[goal.type]?.emoji || '⭐'} {goal.current}/{goal.target}
              </div>
            ))}
          </div>

          {/* 消息提示 */}
          {message && (
            <div style={{
              textAlign: 'center',
              marginTop: '8px',
              fontSize: '16px',
              fontWeight: 'bold',
              color: '#FF6B6B',
            }}>
              {message}
            </div>
          )}
        </div>

        {/* 游戏棋盘 */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: `repeat(${BOARD_SIZE}, ${cellSize}px)`,
          gap: '2px',
          justifyContent: 'center',
          padding: '8px',
          background: 'linear-gradient(135deg, #8B4513 0%, #D2691E 50%, #DEB887 100%)',
          borderRadius: '12px',
          boxShadow: '0 4px 16px rgba(0,0,0,0.3)',
          margin: '0 auto',
          width: 'fit-content'
        }}>
          {gameState.board.map((row, rowIdx) =>
            row.map((cell, colIdx) => {
              const pos = `${rowIdx},${colIdx}`
              const isSelected = selected === pos
              const isRemoved = removedGems.has(pos)
              const gemData = cell.gem !== null ? GEM_COLORS[cell.gem] : null

              return (
                <div
                  key={pos}
                  onClick={() => handleCellClick(pos)}
                  style={{
                    width: `${cellSize}px`,
                    height: `${cellSize}px`,
                    borderRadius: '6px',
                    background: gemData
                      ? `linear-gradient(145deg, ${gemData.primary}, ${gemData.secondary})`
                      : cell.obstacle
                        ? OBSTACLE_INFO[cell.obstacle]?.emoji === '🪨' ? '#666' : '#a8d8ea'
                        : '#A0522D',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    cursor: animating || gameState.status !== 'playing' ? 'default' : 'pointer',
                    transition: 'all 0.15s ease',
                    transform: isSelected ? 'scale(1.1)' : isRemoved ? 'scale(0)' : 'scale(1)',
                    opacity: isRemoved ? 0 : 1,
                    boxShadow: isSelected
                      ? `0 0 0 2px #fff, 0 0 8px rgba(255,215,0,0.8)`
                      : gemData ? `0 2px 4px rgba(0,0,0,0.2)` : 'none',
                    fontSize: `${cellSize * 0.5}px`,
                    position: 'relative',
                    userSelect: 'none',
                  }}
                >
                  {/* 特效指示 */}
                  {cell.special && (
                    <span style={{ position: 'absolute', top: '-4px', right: '-4px', fontSize: '10px' }}>
                      {SPECIAL_EMOJIS[cell.special]}
                    </span>
                  )}

                  {/* 宝石 */}
                  {cell.gem !== null && GEM_EMOJIS[cell.gem]}

                  {/* 障碍物 */}
                  {cell.obstacle && cell.obstacle !== 'rock' && (
                    <span style={{
                      position: 'absolute',
                      top: '50%', left: '50%',
                      transform: 'translate(-50%, -50%)',
                      fontSize: `${cellSize * 0.6}px`,
                    }}>
                      {OBSTACLE_INFO[cell.obstacle]?.emoji}
                    </span>
                  )}

                  {/* 特殊元素 */}
                  {cell.element && (
                    <span style={{
                      position: 'absolute',
                      bottom: '-4px', right: '-4px',
                      fontSize: '12px',
                      zIndex: 2,
                    }}>
                      {ELEMENT_INFO[cell.element]?.emoji}
                    </span>
                  )}
                </div>
              )
            })
          )}
        </div>

        {/* 操作提示 */}
        <div style={{
          textAlign: 'center',
          marginTop: '12px',
          fontSize: '14px',
          color: '#666',
        }}>
          {gameState.status === 'playing'
            ? (selected ? '点击相邻格子交换' : '点击格子开始')
            : ''}
        </div>

        {/* 关卡提示 */}
        {config.type !== 'normal' && gameState.status === 'playing' && (
          <div style={{
            textAlign: 'center',
            marginTop: '4px',
            fontSize: '12px',
            color: config.type === 'difficult' ? '#FF6B6B' : '#9400D3',
          }}>
            {config.type === 'difficult' && '⚠️ 困难关卡，注意步数！'}
            {config.type === 'boss' && '👑 BOSS关卡，全力以赴！'}
          </div>
        )}

        {/* 蜗牛警告 */}
        {gameState.snails.length > 0 && (
          <div style={{
            textAlign: 'center',
            marginTop: '8px',
            fontSize: '12px',
            color: '#E84393',
          }}>
            🐌 蜗牛正在下落，注意消除！
          </div>
        )}

        {/* 游戏结束弹窗 */}
        {gameState.status !== 'playing' && (
          <div style={{
            position: 'fixed',
            top: 0, left: 0, right: 0, bottom: 0,
            background: 'rgba(0,0,0,0.75)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 100
          }}>
            <div style={{
              background: 'linear-gradient(135deg, #FFF8DC, #FFE4B5)',
              padding: '32px',
              borderRadius: '20px',
              textAlign: 'center',
              margin: '16px',
              boxShadow: '0 10px 40px rgba(0,0,0,0.4)'
            }}>
              <div style={{ fontSize: '64px', marginBottom: '12px' }}>
                {gameState.status === 'won' ? '🎉' : '😢'}
              </div>

              <div style={{
                fontSize: '28px',
                fontWeight: 'bold',
                color: gameState.status === 'won' ? '#26DE81' : '#FF6B6B',
                marginBottom: '12px'
              }}>
                {gameState.status === 'won' ? '恭喜过关!' : '挑战失败'}
              </div>

              {/* 星星评价 */}
              {gameState.status === 'won' && (
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '40px' }}>
                    {'⭐'.repeat(gameState.stars)}
                    {'☆'.repeat(3 - gameState.stars)}
                  </div>
                </div>
              )}

              <div style={{
                background: 'rgba(0,0,0,0.05)',
                padding: '12px',
                borderRadius: '10px',
                marginBottom: '20px'
              }}>
                <div style={{ fontSize: '16px', color: '#666' }}>本关得分</div>
                <div style={{ fontSize: '36px', fontWeight: 'bold', color: '#FF6B6B' }}>
                  {gameState.score}
                </div>
                {gameState.status === 'won' && (
                  <div style={{ fontSize: '14px', color: '#26DE81' }}>
                    目标 {config.targetScore} ✓
                  </div>
                )}
              </div>

              <div style={{ display: 'flex', gap: '12px', justifyContent: 'center' }}>
                <button onClick={handleBackToChapter} style={{
                  padding: '12px 20px',
                  borderRadius: '10px',
                  background: '#CCC',
                  border: 'none',
                  fontSize: '16px',
                  cursor: 'pointer'
                }}>返回</button>

                {gameState.status === 'won' ? (
                  <button onClick={handleNextLevel} style={{
                    padding: '12px 20px',
                    borderRadius: '10px',
                    background: 'linear-gradient(135deg, #26DE81, #20BF6B)',
                    border: 'none',
                    fontSize: '16px',
                    color: '#fff',
                    cursor: 'pointer'
                  }}>下一关 →</button>
                ) : (
                  <button onClick={handleRestart} style={{
                    padding: '12px 20px',
                    borderRadius: '10px',
                    background: 'linear-gradient(135deg, #FF6B6B, #EE5A5A)',
                    border: 'none',
                    fontSize: '16px',
                    color: '#fff',
                    cursor: 'pointer'
                  }}>重试</button>
                )}
              </div>
            </div>
          </div>
        )}
      </div>
    )
  }

  return view === 'chapter' ? renderChapterView() : renderGameView()
}
