import type { ReactNode } from 'react'
import { View, Text } from '@tarojs/components'
import Taro from '@tarojs/taro'
import './GameHeader.scss'

interface Props {
  title: string
  right?: ReactNode
}

export function GameHeader ({ title, right }: Props) {
  return (
    <View className='game-header'>
      <View className='game-header-back' onClick={() => Taro.navigateBack()}>←</View>
      <Text className='game-header-title'>{title}</Text>
      <View className='game-header-right'>{right}</View>
    </View>
  )
}
