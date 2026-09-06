/**
 * @file index.tsx
 * @brief 消消乐游戏页面
 */
import { useEffect, useState } from 'react'
import { View, Text } from '@tarojs/components'
import Taro from '@tarojs/taro'
import {
  createGameState,
  findMatches,
  dropGems,
  isValidSwap,
  doSwap,
  calculateScore,
  calculateStars,
  getChapterByLevel,
  GEM_EMOJIS,
  Cell,
  GemType,
  GameState
} from '@/utils/match3'
import {
  setMatch3ChapterStars,
  unlockMatch3Chapter,
  unlockAchievement
} from '@/utils/storage'
import { buildMatch3Share } from '@/services/share'
import './index.scss'

function Match3Page () {
  const [chapterId, setChapterId] = useState<number>(1)
  const [levelId, setLevelId] = useState<number>(1)
  const [moves, setMoves] = useState<number>(0)
  const [score, setScore] = useState<number>(0)
  const [isGameOver, setIsGameOver] = useState<boolean>(false)
  const [gameState, setGameState] = useState<GameState | null>(null)
  const [selectedCell, setSelectedCell] = useState<string | null>(null)
  const [isAnimating, setIsAnimating] = useState<boolean>(false)

  useEffect(() => {
    startNewGame(1)
  }, [])

  // 微信分享回调：小程序环境注册分享信息（包含当前关卡和分数）
  useEffect(() => {
    if (typeof Taro.useShareAppMessage === 'function') {
      Taro.useShareAppMessage(() => buildMatch3Share(chapterId, levelId, score))
    }
  }, [chapterId, levelId, score])

  function startNewGame (level: number) {
    const next = createGameState(level)
    setGameState(next)
    setSelectedCell(null)
    setIsAnimating(false)
    setIsGameOver(false)
    setScore(next.score)
    setMoves(next.moves)
    setLevelId(next.level)
    setChapterId(next.chapter)
  }

  function handleCellClick (row: number, col: number) {
    if (!gameState || isAnimating) return

    const pos = `${row},${col}`

    if (!selectedCell) {
      setSelectedCell(pos)
      return
    }

    const [r1, c1] = selectedCell.split(',').map(Number)
    const [r2, c2] = pos.split(',').map(Number)

    const isAdjacent = Math.abs(r1 - r2) + Math.abs(c1 - c2) === 1

    if (isAdjacent) {
      trySwap(r1, c1, r2, c2)
    }

    setSelectedCell(null)
  }

  function trySwap (r1: number, c1: number, r2: number, c2: number) {
    if (!gameState) return
    const state = gameState

    if (!isValidSwap(state.board, `${r1},${c1}`, `${r2},${c2}`)) {
      return
    }

    doSwap(state.board, `${r1},${c1}`, `${r2},${c2}`)

    const matches = findMatches(state.board)

    if (matches.length === 0) {
      doSwap(state.board, `${r1},${c1}`, `${r2},${c2}`)
      Taro.showToast({ title: '没有匹配', icon: 'none' })
      setGameState({ ...state })
      setSelectedCell(null)
      return
    }

    processMatches()
  }

  function processMatches () {
    if (!gameState) return
    const state = gameState
    setIsAnimating(true)

    let combo = 0
    let matches = findMatches(state.board)

    while (matches.length > 0) {
      combo++

      const toRemove = new Set<string>()
      for (const match of matches) {
        match.cells.forEach(cell => toRemove.add(cell))
      }

      const matchScore = calculateScore(toRemove.size, combo)
      state.score += matchScore

      toRemove.forEach(pos => {
        const [r, c] = pos.split(',').map(Number)
        state.board[r][c].gem = null
      })

      dropGems(state.board)

      state.moves--

      matches = findMatches(state.board)
    }

    state.goals.forEach(goal => {
      if (goal.type === 'score') {
        goal.current = state.score
      }
    })

    setGameState({ ...state })
    setScore(state.score)
    setMoves(state.moves)
    setIsAnimating(false)

    checkGameEnd()
  }

  function checkGameEnd () {
    if (!gameState) return
    const state = gameState

    const goalsComplete = state.goals.every(g => g.current >= g.target)

    if (goalsComplete) {
      state.status = 'won'
      const par = state.goals.find(g => g.type === 'score')?.target || 0
      const stars = calculateStars(state.score, par, state.moves, state.maxMoves)
      state.stars = stars

      // 持久化章节进度 + 成就
      setMatch3ChapterStars(state.chapter, state.level, stars)
      unlockMatch3Chapter(state.chapter + 1)
      unlockAchievement('match3_first_clear')
      if (stars === 3) unlockAchievement('match3_three_stars')

      setGameState({ ...state })
      setIsGameOver(true)
    } else if (state.moves <= 0) {
      state.status = 'lost'
      setGameState({ ...state })
      setIsGameOver(true)
    }
  }

  function handleBack () {
    Taro.navigateBack()
  }

  function handleRestart () {
    if (gameState) {
      startNewGame(gameState.level)
    }
  }

  function renderGem (cell: Cell, row: number, col: number) {
    const pos = `${row},${col}`
    const isSelected = selectedCell === pos

    if (!cell.gem && cell.gem !== 0) {
      return (
        <View
          className={`gem-cell empty ${isSelected ? 'selected' : ''}`}
          onClick={() => handleCellClick(row, col)}
        >
          <Text></Text>
        </View>
      )
    }

    const emoji = GEM_EMOJIS[cell.gem as GemType] || '🟡'

    return (
      <View
        className={`gem-cell ${isSelected ? 'selected' : ''}`}
        onClick={() => handleCellClick(row, col)}
      >
        <Text className="gem-emoji">{emoji}</Text>
      </View>
    )
  }

  if (!gameState) {
    return (
      <View className="match3-page">
        <Text>加载中...</Text>
      </View>
    )
  }

  const chapter = getChapterByLevel(gameState.level)

  return (
    <View className="match3-page">
      <View className="header">
        <View className="info">
          <Text className="info-label">关卡 {gameState.level}</Text>
          <Text className="info-value">{chapter?.name || ''}</Text>
        </View>
        <View className="info">
          <Text className="info-label">得分</Text>
          <Text className="info-value">{gameState.score}</Text>
        </View>
        <View className="info">
          <Text className="info-label">步数</Text>
          <Text className="info-value">{gameState.moves}</Text>
        </View>
      </View>

      <View className="match3-board">
        {gameState.board.map((row, rowIndex) => (
          <View key={rowIndex} className="board-row">
            {row.map((cell, colIndex) => renderGem(cell, rowIndex, colIndex))}
          </View>
        ))}
      </View>

      <View className="goals">
        {gameState.goals.map((goal, index) => (
          <View key={index} className="goal-item">
            <Text>{goal.emoji}</Text>
            <Text>{goal.current}/{goal.target}</Text>
          </View>
        ))}
      </View>

      <View className="action-btns">
        <View className="action-btn" onClick={handleRestart}>🔄 重开</View>
        <View className="action-btn" onClick={handleBack}>🏠 主页</View>
      </View>

      {gameState.status === 'won' && (
        <View className="win-overlay">
          <Text className="win-text-title">🎉 恭喜过关!</Text>
          <Text className="stars">{'⭐'.repeat(gameState.stars)}</Text>
          <Text className="final-score">得分: {gameState.score}</Text>
          <View className="restart-btn" onClick={handleRestart}>再来一局</View>
          <View className="restart-btn" style="margin-top: 20rpx; background: #434343;" onClick={handleBack}>返回主页</View>
        </View>
      )}

      {gameState.status === 'lost' && (
        <View className="game-over-overlay">
          <Text className="game-over-text">游戏结束</Text>
          <Text className="final-score">最终得分: {gameState.score}</Text>
          <View className="restart-btn" onClick={handleRestart}>重新开始</View>
          <View className="restart-btn" style="margin-top: 20rpx; background: #434343;" onClick={handleBack}>返回主页</View>
        </View>
      )}
    </View>
  )
}

// 小程序分享回调由组件内的 useShareAppMessage 注册（见 useEffect）

export default Match3Page