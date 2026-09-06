import { View, Text } from '@tarojs/components'
import './ScoreBoard.scss'

/**
 * @brief 通用得分板：当前得分 + 最高分 + 剩余步数（可选）。
 * @note 三栏布局，调用方可选择不传 moves（仅展示分数）。
 */
interface Props {
  score: number
  best: number
  moves?: number
  label?: string
}

export function ScoreBoard ({ score, best, moves, label = '得分' }: Props) {
  return (
    <View className='score-board'>
      <View className='score-block'>
        <Text className='score-label'>{label}</Text>
        <Text className='score-value'>{score}</Text>
      </View>
      <View className='score-block best'>
        <Text className='score-label'>最高</Text>
        <Text className='score-value'>{best}</Text>
      </View>
      {moves !== undefined && (
        <View className='score-block moves'>
          <Text className='score-label'>步数</Text>
          <Text className='score-value'>{moves}</Text>
        </View>
      )}
    </View>
  )
}