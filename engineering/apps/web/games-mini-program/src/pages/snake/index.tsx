import { useState, useEffect, useCallback, useRef } from 'react'
import { View, Text } from '@tarojs/components'
import Taro from '@tarojs/taro'
import {
  createInitialState,
  tick,
  setDirection,
  getGrid,
  Direction,
  SnakeState as SnakeStateType
} from '@/utils/snake'
import './index.scss'

function SnakePage () {
  const [gameState, setGameState] = useState<SnakeStateType>(() => {
    // 使用函数初始化，确保只执行一次
    return createInitialState()
  })

  const timerRef = useRef<ReturnType<typeof setInterval> | null>(null)
  const touchStartRef = useRef({ x: 0, y: 0 })

  // 启动定时器
  const startTimer = useCallback((speed: number) => {
    if (timerRef.current) {
      clearInterval(timerRef.current)
    }
    timerRef.current = setInterval(() => {
      setGameState(prevState => {
        if (prevState.isGameOver) {
          if (timerRef.current) {
            clearInterval(timerRef.current)
            timerRef.current = null
          }
          return prevState
        }
        // 创建新状态对象（tick 会修改传入的状态）
        const newState = { ...prevState }
        tick(newState)
        return newState
      })
    }, speed)
  }, [])

  // 停止定时器
  const stopTimer = useCallback(() => {
    if (timerRef.current) {
      clearInterval(timerRef.current)
      timerRef.current = null
    }
  }, [])

  // 组件挂载时启动游戏
  useEffect(() => {
    // 初始状态已经在 useState 中创建，直接启动
    startTimer(gameState.speed)
    return () => {
      stopTimer()
    }
  }, [])

  // 开始新游戏
  const startGame = useCallback(() => {
    stopTimer()
    const newState = createInitialState()
    setGameState(newState)
    // 延迟启动定时器，确保状态已更新
    setTimeout(() => {
      startTimer(newState.speed)
    }, 0)
  }, [stopTimer, startTimer])

  // 处理方向
  const handleDirection = useCallback((dir: Direction) => {
    if (gameState.isGameOver) return
    setGameState(prev => {
      const newState = { ...prev }
      setDirection(newState, dir)
      return newState
    })
  }, [gameState.isGameOver])

  // 处理触摸开始
  const handleTouchStart = useCallback((e: any) => {
    touchStartRef.current = {
      x: e.touches[0].clientX,
      y: e.touches[0].clientY
    }
  }, [])

  // 处理触摸结束
  const handleTouchEnd = useCallback((e: any) => {
    if (gameState.isGameOver) return

    const touchEndX = e.changedTouches[0].clientX
    const touchEndY = e.changedTouches[0].clientY
    const dx = touchEndX - touchStartRef.current.x
    const dy = touchEndY - touchStartRef.current.y
    const absDx = Math.abs(dx)
    const absDy = Math.abs(dy)

    if (Math.max(absDx, absDy) < 30) return

    if (absDx > absDy) {
      handleDirection(dx > 0 ? Direction.RIGHT : Direction.LEFT)
    } else {
      handleDirection(dy > 0 ? Direction.DOWN : Direction.UP)
    }
  }, [gameState.isGameOver, handleDirection])

  // 渲染网格
  const grid = getGrid(gameState)

  return (
    <View
      className='snake-page'
      onTouchStart={handleTouchStart}
      onTouchEnd={handleTouchEnd}
    >
      <View className='header'>
        <Text className='score'>得分: <Text className='score-value'>{gameState.score}</Text></Text>
        <Text className='score'>速度: {Math.round(150 / gameState.speed * 100)}%</Text>
      </View>

      <View className='game-board'>
        {grid.map((row, rowIndex) => (
          <View key={rowIndex} className='grid-row'>
            {row.map((cell, colIndex) => (
              <View
                key={colIndex}
                className={`grid-cell ${
                  cell === 1 ? 'snake' :
                  cell === 2 ? 'food' :
                  cell === 3 ? 'snake-head' : ''
                }`}
              />
            ))}
          </View>
        ))}
      </View>

      <View className='controls'>
        <View className='direction-pad'>
          <View className='dir-btn up' onClick={() => handleDirection(Direction.UP)}>↑</View>
          <View className='dir-btn left' onClick={() => handleDirection(Direction.LEFT)}>←</View>
          <View className='dir-btn right' onClick={() => handleDirection(Direction.RIGHT)}>→</View>
          <View className='dir-btn down' onClick={() => handleDirection(Direction.DOWN)}>↓</View>
        </View>
        <Text className='hint'>使用方向键或滑动屏幕控制蛇的移动</Text>
      </View>

      {gameState.isGameOver && (
        <View className='game-over-overlay'>
          <Text className='game-over-text'>游戏结束</Text>
          <Text className='final-score'>最终得分: {gameState.score}</Text>
          <View className='restart-btn' onClick={startGame}>重新开始</View>
          <View className='restart-btn' style='margin-top: 20rpx; background: #434343;' onClick={() => Taro.navigateBack()}>返回主页</View>
        </View>
      )}
    </View>
  )
}

export default SnakePage
