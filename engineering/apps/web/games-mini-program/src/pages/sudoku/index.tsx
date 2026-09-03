import { Component } from 'react'
import { View, Text } from '@tarojs/components'
import Taro from '@tarojs/taro'
import {
  createInitialState,
  placeNumber,
  eraseCell,
  moveCursor,
  selectNumber,
  hint,
  checkWin,
  SUDOKU_CONFIG
} from '@/utils/sudoku'
import './index.scss'

interface Cell {
  value: number
  isGiven: boolean
  isConflict: boolean
}

interface SudokuState {
  board: Cell[][]
  solution: number[][]
  cursorRow: number
  cursorCol: number
  difficulty: number
  emptyCount: number
  isGameOver: boolean
  selectedNumber: number
  hintCount: number  // 提示次数限制
  startTime: number  // 开始时间
  elapsedTime: number  // 已用时间（秒）
}

class SudokuPage extends Component {
  state: { gameState: SudokuState } = {
    gameState: createInitialState(0) as SudokuState
  }

  timer: NodeJS.Timeout | null = null

  componentDidMount() {
    this.startTimer()
  }

  componentWillUnmount() {
    this.stopTimer()
  }

  startTimer = () => {
    this.stopTimer()
    const { gameState } = this.state
    gameState.startTime = Date.now()
    gameState.elapsedTime = 0
    this.timer = setInterval(() => {
      const { gameState } = this.state
      if (!gameState.isGameOver) {
        gameState.elapsedTime = Math.floor((Date.now() - gameState.startTime) / 1000)
        this.setState({ gameState: { ...gameState } })
      }
    }, 1000)
  }

  stopTimer = () => {
    if (this.timer) {
      clearInterval(this.timer)
      this.timer = null
    }
  }

  handleDifficultyChange = (difficulty: number) => {
    this.stopTimer()
    this.setState({ gameState: createInitialState(difficulty) as SudokuState }, () => {
      this.startTimer()
    })
  }

  handleNumberSelect = (num: number) => {
    const { gameState } = this.state
    selectNumber(gameState, num)
    placeNumber(gameState, num)
    checkWin(gameState)
    this.setState({ gameState: { ...gameState } })
  }

  handleErase = () => {
    const { gameState } = this.state
    eraseCell(gameState)
    this.setState({ gameState: { ...gameState } })
  }

  handleHint = () => {
    const { gameState } = this.state
    if (gameState.hintCount <= 0) {
      Taro.showToast({ title: '提示次数已用完', icon: 'none' })
      return
    }
    const success = hint(gameState)
    if (!success) {
      Taro.showToast({ title: '没有空格了', icon: 'none' })
    } else {
      checkWin(gameState)
    }
    this.setState({ gameState: { ...gameState } })
  }

  handleCellClick = (row: number, col: number) => {
    const { gameState } = this.state
    const cell = gameState.board[row][col]

    // 固定格点击提示
    if (cell.isGiven) {
      Taro.showToast({ title: '这是固定数字，无法修改', icon: 'none' })
      return
    }

    // 移动光标到目标位置
    gameState.cursorRow = row
    gameState.cursorCol = col

    // 如果已经有选中的数字，自动填入
    if (gameState.selectedNumber > 0) {
      placeNumber(gameState, gameState.selectedNumber)
    }

    this.setState({ gameState: { ...gameState } })
  }

  handleRestart = () => {
    const { gameState } = this.state
    this.stopTimer()
    this.setState({ gameState: createInitialState(gameState.difficulty) as SudokuState }, () => {
      this.startTimer()
    })
  }

  handleBack = () => {
    this.stopTimer()
    Taro.navigateBack()
  }

  // 处理触摸滑动
  handleTouchStart = (e: any) => {
    this.touchStartX = e.touches[0].clientX
    this.touchStartY = e.touches[0].clientY
  }

  handleTouchEnd = (e: any) => {
    const { gameState } = this.state
    if (gameState.isGameOver) return

    const touchEndX = e.changedTouches[0].clientX
    const touchEndY = e.changedTouches[0].clientY
    const dx = touchEndX - this.touchStartX
    const dy = touchEndY - this.touchStartY
    const absDx = Math.abs(dx)
    const absDy = Math.abs(dy)

    if (Math.max(absDx, absDy) < 30) return

    if (absDx > absDy) {
      moveCursor(gameState, dx > 0 ? 1 : -1, 0)
    } else {
      moveCursor(gameState, 0, dy > 0 ? 1 : -1)
    }
    checkWin(gameState)
    this.setState({ gameState: { ...gameState } })
  }

  // 计算星星数量
  calculateStars = (): number => {
    const { gameState } = this.state
    const { elapsedTime, difficulty, hintCount } = gameState
    const diffConfig = SUDOKU_CONFIG.DIFFICULTIES[difficulty]
    const maxTime = diffConfig.maxTime  // 每种难度的目标时间

    // 无提示 + 时间短 = 3星
    if (hintCount === 3 && elapsedTime <= maxTime * 0.5) return 3
    // 有提示但不多 + 时间合适 = 2星
    if (hintCount >= 1 && elapsedTime <= maxTime) return 2
    // 完成就至少1星
    return 1
  }

  // 格式化时间
  formatTime = (seconds: number): string => {
    const mins = Math.floor(seconds / 60)
    const secs = seconds % 60
    return `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`
  }

  private touchStartX = 0
  private touchStartY = 0

  render () {
    const { gameState } = this.state
    const { board, cursorRow, cursorCol, difficulty, emptyCount, selectedNumber, hintCount, elapsedTime, isGameOver } = gameState

    return (
      <View
        className='sudoku-page'
        onTouchStart={this.handleTouchStart}
        onTouchEnd={this.handleTouchEnd}
      >
        {/* 顶部状态栏 */}
        <View className='status-bar'>
          <View className='status-item'>
            <Text className='status-label'>⏱️ 用时</Text>
            <Text className='status-value'>{this.formatTime(elapsedTime)}</Text>
          </View>
          <View className='status-item'>
            <Text className='status-label'>💡 提示</Text>
            <Text className='status-value hint-value'>{hintCount}/3</Text>
          </View>
          <View className='status-item'>
            <Text className='status-label'>⬜ 空格</Text>
            <Text className='status-value'>{emptyCount}</Text>
          </View>
        </View>

        {/* 难度选择器 */}
        <View className='difficulty-selector'>
          {SUDOKU_CONFIG.DIFFICULTIES.map((diff, index) => (
            <View
              key={index}
              className={`difficulty-btn ${difficulty === index ? 'active' : ''}`}
              onClick={() => this.handleDifficultyChange(index)}
            >
              <Text className='diff-name'>{diff.name}</Text>
              <Text className='diff-hint'>{diff.description}</Text>
            </View>
          ))}
        </View>

        {/* 数独棋盘 */}
        <View className='sudoku-board'>
          {board.map((row, rowIndex) => (
            <View key={rowIndex} className='sudoku-row'>
              {row.map((cell, colIndex) => (
                <View
                  key={colIndex}
                  className={`sudoku-cell ${
                    cell.isGiven ? 'given' : ''
                  } ${
                    rowIndex === cursorRow && colIndex === cursorCol ? 'cursor' : ''
                  } ${
                    cell.isConflict ? 'conflict' : ''
                  }`}
                  onClick={() => this.handleCellClick(rowIndex, colIndex)}
                >
                  <Text>{cell.value || ''}</Text>
                </View>
              ))}
            </View>
          ))}
        </View>

        {/* 数字键盘 */}
        <View className='number-pad'>
          {[1, 2, 3, 4, 5, 6, 7, 8, 9].map(num => (
            <View
              key={num}
              className={`num-btn ${selectedNumber === num ? 'selected' : ''}`}
              onClick={() => this.handleNumberSelect(num)}
            >
              {num}
            </View>
          ))}
          <View className='num-btn erase-btn' onClick={this.handleErase}>×</View>
        </View>

        {/* 操作按钮 */}
        <View className='action-bar'>
          <View className='action-btn hint-btn' onClick={this.handleHint}>
            <Text>💡 提示</Text>
          </View>
          <View className='action-btn restart-btn' onClick={this.handleRestart}>
            <Text>🔄 重开</Text>
          </View>
          <View className='action-btn back-btn' onClick={this.handleBack}>
            <Text>🏠 主页</Text>
          </View>
        </View>

        {/* 游戏完成弹窗 */}
        {isGameOver && (
          <View className='game-complete-overlay'>
            <View className='complete-card'>
              <Text className='complete-title'>🎉 恭喜通关!</Text>

              {/* 星星评价 */}
              <View className='stars-container'>
                {[1, 2, 3].map(star => (
                  <Text key={star} className={`star ${star <= this.calculateStars() ? 'earned' : ''}`}>
                    ★
                  </Text>
                ))}
              </View>

              <View className='complete-stats'>
                <View className='stat-row'>
                  <Text className='stat-label'>用时</Text>
                  <Text className='stat-value'>{this.formatTime(elapsedTime)}</Text>
                </View>
                <View className='stat-row'>
                  <Text className='stat-label'>难度</Text>
                  <Text className='stat-value'>{SUDOKU_CONFIG.DIFFICULTIES[difficulty].name}</Text>
                </View>
                <View className='stat-row'>
                  <Text className='stat-label'>使用提示</Text>
                  <Text className='stat-value'>{3 - hintCount} 次</Text>
                </View>
              </View>

              <View className='complete-actions'>
                <View className='complete-btn primary' onClick={this.handleRestart}>
                  <Text>再来一局</Text>
                </View>
                <View className='complete-btn secondary' onClick={this.handleBack}>
                  <Text>返回主页</Text>
                </View>
              </View>
            </View>
          </View>
        )}
      </View>
    )
  }
}

export default SudokuPage