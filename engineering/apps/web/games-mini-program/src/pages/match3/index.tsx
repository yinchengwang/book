/**
 * @file index.tsx
 * @brief 消消乐游戏页面
 */
import { Component } from 'react'
import { View, Text } from '@tarojs/components'
import Taro from '@tarojs/taro'
import {
  createGameState,
  createBoard,
  findMatches,
  dropGems,
  isValidSwap,
  doSwap,
  calculateScore,
  calculateStars,
  getChapterByLevel,
  CHAPTERS,
  GEM_EMOJIS,
  BOARD_SIZE,
  Cell,
  GemType,
  GameState
} from '@/utils/match3'
import './index.scss'

class Match3Page extends Component {
  state: { gameState: GameState | null; selectedCell: string | null; isAnimating: boolean }

  constructor(props: any) {
    super(props)
    this.state = {
      gameState: null,
      selectedCell: null,
      isAnimating: false
    }
  }

  componentDidMount() {
    this.startNewGame(1)
  }

  startNewGame = (level: number) => {
    const gameState = createGameState(level)
    this.setState({ gameState, selectedCell: null, isAnimating: false })
  }

  handleCellClick = (row: number, col: number) => {
    const { gameState, selectedCell, isAnimating } = this.state
    if (!gameState || isAnimating) return

    const pos = `${row},${col}`

    if (!selectedCell) {
      // 第一次点击，选中格子
      this.setState({ selectedCell: pos })
    } else {
      // 第二次点击，尝试交换
      const [r1, c1] = selectedCell.split(',').map(Number)
      const [r2, c2] = pos.split(',').map(Number)

      // 检查是否相邻
      const isAdjacent = Math.abs(r1 - r2) + Math.abs(c1 - c2) === 1

      if (isAdjacent) {
        this.trySwap(r1, c1, r2, c2)
      }

      this.setState({ selectedCell: null })
    }
  }

  trySwap = (r1: number, c1: number, r2: number, c2: number) => {
    const { gameState } = this.state
    if (!gameState) return

    // 检查是否有效交换
    if (!isValidSwap(gameState.board, `${r1},${c1}`, `${r2},${c2}`)) {
      return
    }

    // 先执行交换
    doSwap(gameState.board, `${r1},${c1}`, `${r2},${c2}`)

    // 检查是否有匹配
    const matches = findMatches(gameState.board)

    if (matches.length === 0) {
      // 无匹配，弹回
      doSwap(gameState.board, `${r1},${c1}`, `${r2},${c2}`)
      Taro.showToast({ title: '没有匹配', icon: 'none' })
      this.setState({ gameState: { ...gameState }, selectedCell: null })
    } else {
      // 有匹配，处理消除
      this.processMatches()
    }
  }

  processMatches = () => {
    const { gameState } = this.state
    if (!gameState) return

    this.setState({ isAnimating: true })

    let combo = 0
    let matches = findMatches(gameState.board)

    while (matches.length > 0) {
      combo++

      // 收集所有需要消除的格子
      const toRemove = new Set<string>()
      for (const match of matches) {
        match.cells.forEach(cell => toRemove.add(cell))
      }

      // 计算分数
      const score = calculateScore(toRemove.size, combo)
      gameState.score += score

      // 消除格子
      toRemove.forEach(pos => {
        const [r, c] = pos.split(',').map(Number)
        gameState.board[r][c].gem = null
      })

      // 宝石下落
      dropGems(gameState.board)

      // 减少步数
      gameState.moves--

      // 继续检查新的匹配
      matches = findMatches(gameState.board)
    }

    // 更新目标进度
    gameState.goals.forEach(goal => {
      if (goal.type === 'score') {
        goal.current = gameState.score
      }
    })

    // 更新状态
    this.setState({ gameState: { ...gameState }, isAnimating: false })

    // 检查游戏是否结束
    this.checkGameEnd()
  }

  checkGameEnd = () => {
    const { gameState } = this.state
    if (!gameState) return

    // 检查是否达成目标
    const goalsComplete = gameState.goals.every(
      g => g.current >= g.target
    )

    if (goalsComplete) {
      gameState.status = 'won'
      const stars = calculateStars(
        gameState.score,
        gameState.goals.find(g => g.type === 'score')?.target || 0,
        gameState.moves,
        gameState.maxMoves
      )
      gameState.stars = stars
      this.setState({ gameState: { ...gameState } })
    } else if (gameState.moves <= 0) {
      gameState.status = 'lost'
      this.setState({ gameState: { ...gameState } })
    }
  }

  handleBack = () => {
    Taro.navigateBack()
  }

  handleRestart = () => {
    const { gameState } = this.state
    if (gameState) {
      this.startNewGame(gameState.level)
    }
  }

  renderGem = (cell: Cell, row: number, col: number) => {
    const { selectedCell } = this.state
    const pos = `${row},${col}`
    const isSelected = selectedCell === pos

    if (!cell.gem && cell.gem !== 0) {
      return (
        <View
          className={`gem-cell empty ${isSelected ? 'selected' : ''}`}
          onClick={() => this.handleCellClick(row, col)}
        >
          <Text></Text>
        </View>
      )
    }

    const emoji = GEM_EMOJIS[cell.gem as GemType] || '🟡'

    return (
      <View
        className={`gem-cell ${isSelected ? 'selected' : ''}`}
        onClick={() => this.handleCellClick(row, col)}
      >
        <Text className="gem-emoji">{emoji}</Text>
      </View>
    )
  }

  render() {
    const { gameState } = this.state

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
              {row.map((cell, colIndex) => this.renderGem(cell, rowIndex, colIndex))}
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
          <View className="action-btn" onClick={this.handleRestart}>🔄 重开</View>
          <View className="action-btn" onClick={this.handleBack}>🏠 主页</View>
        </View>

        {gameState.status === 'won' && (
          <View className="win-overlay">
            <Text className="win-text-title">🎉 恭喜过关!</Text>
            <Text className="stars">{'⭐'.repeat(gameState.stars)}</Text>
            <Text className="final-score">得分: {gameState.score}</Text>
            <View className="restart-btn" onClick={this.handleRestart}>再来一局</View>
            <View className="restart-btn" style="margin-top: 20rpx; background: #434343;" onClick={this.handleBack}>返回主页</View>
          </View>
        )}

        {gameState.status === 'lost' && (
          <View className="game-over-overlay">
            <Text className="game-over-text">游戏结束</Text>
            <Text className="final-score">最终得分: {gameState.score}</Text>
            <View className="restart-btn" onClick={this.handleRestart}>重新开始</View>
            <View className="restart-btn" style="margin-top: 20rpx; background: #434343;" onClick={this.handleBack}>返回主页</View>
          </View>
        )}
      </View>
    )
  }
}

export default Match3Page