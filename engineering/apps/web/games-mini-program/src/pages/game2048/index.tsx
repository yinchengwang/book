import { Component } from 'react'
import { View, Text } from '@tarojs/components'
import Taro from '@tarojs/taro'
import {
  createInitialState,
  move,
  keepGoing,
  GAME2048_CONFIG
} from '@/utils/2048'
import './index.scss'

interface Game2048State {
  board: number[][]
  score: number
  bestScore: number
  isGameOver: boolean
  hasWon: boolean
  keepGoing: boolean
}

class Game2048Page extends Component {
  state: { gameState: Game2048State } = {
    gameState: createInitialState() as Game2048State
  }

  handleMove = (direction: 'left' | 'right' | 'up' | 'down') => {
    const { gameState } = this.state
    if (!gameState.isGameOver) {
      move(gameState, direction)
      this.setState({ gameState: { ...gameState } })
    }
  }

  handleRestart = () => {
    const { gameState } = this.state
    this.setState({ gameState: createInitialState(gameState.bestScore) })
  }

  handleKeepGoing = () => {
    const { gameState } = this.state
    keepGoing(gameState)
    this.setState({ gameState: { ...gameState } })
  }

  handleBack = () => {
    Taro.navigateBack()
  }

  // 处理触摸滑动
  handleTouchStart = (e: any) => {
    this.touchStartX = e.touches[0].clientX
    this.touchStartY = e.touches[0].clientY
  }

  handleTouchEnd = (e: any) => {
    const { gameState } = this.state
    if (gameState.isGameOver || gameState.hasWon && !gameState.keepGoing) return

    // 防抖：快速连续滑动只处理第一次
    const now = Date.now()
    if (now - this.lastMoveTime < this.MOVE_DEBOUNCE) return
    this.lastMoveTime = now

    const touchEndX = e.changedTouches[0].clientX
    const touchEndY = e.changedTouches[0].clientY
    const dx = touchEndX - this.touchStartX
    const dy = touchEndY - this.touchStartY
    const absDx = Math.abs(dx)
    const absDy = Math.abs(dy)

    if (Math.max(absDx, absDy) < 30) return

    if (absDx > absDy) {
      this.handleMove(dx > 0 ? 'right' : 'left')
    } else {
      this.handleMove(dy > 0 ? 'down' : 'up')
    }
  }

  private touchStartX = 0
  private touchStartY = 0
  private lastMoveTime = 0
  private readonly MOVE_DEBOUNCE = 100 // 防抖间隔（毫秒）

  // 获取 tile 样式类名
  getTileClass = (value: number): string => {
    if (value > 2048) return 'tile tile-super'
    return `tile tile-${value}`
  }

  render () {
    const { gameState } = this.state
    const { board, score, bestScore, isGameOver, hasWon, keepGoing } = gameState

    return (
      <View
        className='game2048-page'
        onTouchStart={this.handleTouchStart}
        onTouchEnd={this.handleTouchEnd}
      >
        <View className='header'>
          <View className='score-box'>
            <Text className='score-label'>分数</Text>
            <Text className='score-value'>{score}</Text>
          </View>
          <View className='score-box'>
            <Text className='score-label'>最高分</Text>
            <Text className='score-value'>{bestScore}</Text>
          </View>
        </View>

        <View className='game2048-board'>
          {board.map((row, rowIndex) => (
            <View key={rowIndex} className='board-row'>
              {row.map((value, colIndex) => (
                <View key={colIndex} className={this.getTileClass(value)}>
                  <Text>{value || ''}</Text>
                </View>
              ))}
            </View>
          ))}
        </View>

        <View className='direction-pad'>
          <View className='dir-btn up' onClick={() => this.handleMove('up')}>↑</View>
          <View className='dir-btn left' onClick={() => this.handleMove('left')}>←</View>
          <View className='dir-btn right' onClick={() => this.handleMove('right')}>→</View>
          <View className='dir-btn down' onClick={() => this.handleMove('down')}>↓</View>
        </View>

        <View className='action-btns'>
          <View className='action-btn' onClick={this.handleRestart}>🔄 新游戏</View>
          <View className='action-btn' onClick={this.handleBack}>🏠 主页</View>
        </View>

        <Text className='hint'>滑动屏幕或点击方向键移动方块</Text>

        {/* 达成 2048 弹窗 */}
        {hasWon && !keepGoing && (
          <View className='win-overlay'>
            <Text className='win-text-title'>🎉 你赢了!</Text>
            <View className='restart-btn' onClick={this.handleKeepGoing}>继续游戏</View>
            <View className='restart-btn' style='margin-top: 20rpx; background: #434343;' onClick={this.handleRestart}>重新开始</View>
          </View>
        )}

        {/* 游戏结束弹窗 */}
        {isGameOver && (
          <View className='game-over-overlay'>
            <Text className='game-over-text'>游戏结束</Text>
            <Text className='final-score'>最终得分: {score}</Text>
            <View className='restart-btn' onClick={this.handleRestart}>再来一局</View>
            <View className='restart-btn' style='margin-top: 20rpx; background: #434343;' onClick={this.handleBack}>返回主页</View>
          </View>
        )}
      </View>
    )
  }
}

export default Game2048Page