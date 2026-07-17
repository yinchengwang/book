import React from 'react'
import { useState } from 'react'
import Match3Game from './components/Match3Game'

// 游戏配置
const GAMES = [
  { id: 'snake', name: '贪吃蛇', desc: '经典怀旧，操控蛇吃食物', icon: '🐍', color: '#11998e' },
  { id: 'sudoku', name: '数独', desc: '烧脑益智，填满 1-9 数字', icon: '🔢', color: '#667eea' },
  { id: '2048', name: '2048', desc: '合并数字，挑战 2048', icon: '🎮', color: '#f5576c' },
  { id: 'match3', name: '消消乐', desc: '宝石消除，连线三个以上', icon: '💎', color: '#e94560' }
]

function App() {
  const [currentPage, setCurrentPage] = useState<string | null>(null)

  const handleGameClick = (gameId: string) => {
    setCurrentPage(gameId)
  }

  const handleBack = () => {
    setCurrentPage(null)
  }

  // 首页
  if (!currentPage) {
    return (
      <div style={{
        minHeight: '100vh',
        background: '#16213e',
        color: '#fff',
        fontFamily: '-apple-system, BlinkMacSystemFont, sans-serif',
        padding: '40rpx',
        boxSizing: 'border-box'
      }}>
        <div style={{ textAlign: 'center', padding: '60rpx 0' }}>
          <h1 style={{ fontSize: '48rpx', color: '#00d9ff', marginBottom: '16rpx' }}>🎮 游戏中心</h1>
          <p style={{ fontSize: '28rpx', color: '#8b8b8b' }}>选择你喜欢的游戏开始吧</p>
        </div>

        <div style={{ marginTop: '60rpx' }}>
          {GAMES.map(game => (
            <div
              key={game.id}
              onClick={() => handleGameClick(game.id)}
              style={{
                background: `linear-gradient(135deg, rgba(255,255,255,0.1) 0%, rgba(255,255,255,0.05) 100%)`,
                borderRadius: '24rpx',
                padding: '40rpx',
                display: 'flex',
                alignItems: 'center',
                marginBottom: '32rpx',
                cursor: 'pointer',
                transition: 'all 0.3s ease'
              }}
            >
              <div style={{
                width: '120rpx',
                height: '120rpx',
                borderRadius: '24rpx',
                background: `linear-gradient(135deg, ${game.color} 0%, ${game.color}99 100%)`,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontSize: '60rpx',
                marginRight: '32rpx'
              }}>
                {game.icon}
              </div>
              <div style={{ flex: 1 }}>
                <div style={{ fontSize: '36rpx', fontWeight: 'bold', marginBottom: '8rpx' }}>{game.name}</div>
                <div style={{ fontSize: '26rpx', color: '#8b8b8b' }}>{game.desc}</div>
              </div>
              <div style={{ fontSize: '40rpx', color: '#8b8b8b' }}>›</div>
            </div>
          ))}
        </div>
      </div>
    )
  }

  // 消消乐游戏页面
  if (currentPage === 'match3') {
    return <Match3Game onBack={handleBack} />
  }

  // 其他游戏页面（占位）
  return (
    <div style={{
      minHeight: '100vh',
      background: '#16213e',
      color: '#fff',
      fontFamily: '-apple-system, BlinkMacSystemFont, sans-serif',
      padding: '40rpx',
      boxSizing: 'border-box'
    }}>
      <div style={{ textAlign: 'center', padding: '60rpx 0' }}>
        <h1 style={{ fontSize: '48rpx', color: '#00d9ff' }}>
          {GAMES.find(g => g.id === currentPage)?.name}
        </h1>
        <p style={{ fontSize: '28rpx', color: '#8b8b8b', marginTop: '20rpx' }}>
          {GAMES.find(g => g.id === currentPage)?.icon} 游戏页面开发中...
        </p>
      </div>

      <div style={{ textAlign: 'center', marginTop: '100rpx' }}>
        <button
          onClick={handleBack}
          style={{
            padding: '24rpx 60rpx',
            borderRadius: '16rpx',
            background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
            color: '#fff',
            fontSize: '32rpx',
            border: 'none',
            cursor: 'pointer'
          }}
        >
          返回主页
        </button>
      </div>
    </div>
  )
}

export default App
