import { View, Text } from '@tarojs/components'
import './PauseModal.scss'

interface Props {
  visible: boolean
  onResume: () => void
  onQuit: () => void
  onRestart: () => void
}

export function PauseModal ({ visible, onResume, onQuit, onRestart }: Props) {
  if (!visible) return null

  return (
    <View className='pause-overlay'>
      <View className='pause-card'>
        <Text className='pause-title'>已暂停</Text>
        <View className='pause-btn primary' onClick={onResume}>继续</View>
        <View className='pause-btn' onClick={onRestart}>重开</View>
        <View className='pause-btn secondary' onClick={onQuit}>退出</View>
      </View>
    </View>
  )
}
