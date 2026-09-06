/**
 * @file components/common/AchievementToast.tsx
 * @brief 解锁成就时的顶部弹窗
 */
import { useEffect, useState } from 'react'
import { View, Text } from '@tarojs/components'
import { subscribe } from '@/services/achievements'
import type { Achievement } from '@/types/achievements'
import './AchievementToast.scss'

const LABELS: Record<string, string> = {
  snake_first_blood: '初次进食',
  snake_score_100: '贪吃蛇 100 分',
  sudoku_first_clear: '首回数独',
  sudoku_no_hint: '无提示通关',
  game2048_reach_2048: '合出 2048',
  game2048_reach_4096: '合出 4096',
  match3_first_clear: '首次通关消消乐',
  match3_three_stars: '消消乐三星'
}

export function AchievementToast () {
  const [current, setCurrent] = useState<Achievement | null>(null)

  useEffect(() => {
    let timer: ReturnType<typeof setTimeout> | null = null
    const unsub = subscribe(a => {
      setCurrent(a)
      if (timer) clearTimeout(timer)
      timer = setTimeout(() => setCurrent(null), 2400)
    })
    return () => {
      unsub()
      if (timer) clearTimeout(timer)
    }
  }, [])

  if (!current) return null
  return (
    <View className='achievement-toast'>
      <Text className='achievement-icon'>🏆</Text>
      <View className='achievement-body'>
        <Text className='achievement-label'>成就解锁</Text>
        <Text className='achievement-name'>{LABELS[current.id] ?? current.id}</Text>
      </View>
    </View>
  )
}

export default AchievementToast