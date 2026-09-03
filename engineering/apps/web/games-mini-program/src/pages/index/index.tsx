import { Component } from 'react'
import { View, Text } from '@tarojs/components'
import Taro from '@tarojs/taro'
import './index.scss'

// 游戏列表配置
const GAMES = [
  {
    id: 'snake',
    name: '贪吃蛇',
    desc: '经典怀旧，操控蛇吃食物',
    icon: '🐍',
    path: '/pages/snake/index'
  },
  {
    id: 'sudoku',
    name: '数独',
    desc: '烧脑益智，填满 1-9 数字',
    icon: '🔢',
    path: '/pages/sudoku/index'
  },
  {
    id: 'game2048',
    name: '2048',
    desc: '合并数字，挑战 2048',
    icon: '🎮',
    path: '/pages/game2048/index'
  },
  {
    id: 'match3',
    name: '消消乐',
    desc: '匹配宝石，闯关冒险',
    icon: '💎',
    path: '/pages/match3/index'
  }
]

class Index extends Component {
  handleGameClick (game: typeof GAMES[0]) {
    Taro.navigateTo({
      url: game.path
    })
  }

  render () {
    return (
      <View className='index-page'>
        <View className='header'>
          <Text className='title'>🎮 游戏中心</Text>
          <Text className='subtitle'>选择你喜欢的游戏开始吧</Text>
        </View>

        <View className='games-grid'>
          {GAMES.map(game => (
            <View
              key={game.id}
              className='game-card'
              onClick={() => this.handleGameClick(game)}
            >
              <View className={`game-icon ${game.id}`}>
                <Text>{game.icon}</Text>
              </View>
              <View className='game-info'>
                <Text className='game-name'>{game.name}</Text>
                <Text className='game-desc'>{game.desc}</Text>
              </View>
              <Text className='arrow'>›</Text>
            </View>
          ))}
        </View>
      </View>
    )
  }
}

export default Index