import { View, Text } from '@tarojs/components'
import './ScoreBoard.scss'

interface Props {
  score: number
  best: number
  label?: string
}

export function ScoreBoard ({ score, best, label = '得分' }: Props) {
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
    </View>
  )
}
