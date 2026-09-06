import { describe, it, expect, beforeEach, vi } from 'vitest'

vi.mock('@tarojs/taro', () => {
  const store: Record<string, string> = {}
  return {
    default: {
      getStorageSync: vi.fn((k: string) => store[k]),
      setStorageSync: vi.fn((k: string, v: string) => { store[k] = v }),
      removeStorageSync: vi.fn((k: string) => { delete store[k] })
    }
  }
})

import { evaluate, subscribe, reset } from './achievements'
import { getAchievements } from '@/utils/storage'

describe('成就规则引擎', () => {
  beforeEach(() => {
    reset()
  })

  it('首次贪吃蛇得分应解锁 first_blood', () => {
    const unlocked = evaluate({ type: 'snake.eat', score: 10 })
    expect(unlocked.some(a => a.id === 'snake_first_blood')).toBe(true)
  })

  it('贪吃蛇累计 100 分应解锁 score_100', () => {
    evaluate({ type: 'snake.eat', score: 50 })
    const unlocked = evaluate({ type: 'snake.gameOver', score: 60 })
    expect(unlocked.some(a => a.id === 'snake_score_100')).toBe(false)

    const unlocked2 = evaluate({ type: 'snake.gameOver', score: 100 })
    expect(unlocked2.some(a => a.id === 'snake_score_100')).toBe(true)
  })

  it('重复触发不会重复解锁', () => {
    evaluate({ type: 'snake.eat', score: 10 })
    const second = evaluate({ type: 'snake.eat', score: 10 })
    expect(second.some(a => a.id === 'snake_first_blood')).toBe(false)
  })

  it('订阅者收到解锁事件', () => {
    const seen: string[] = []
    subscribe(a => seen.push(a.id))
    evaluate({ type: 'game2048.merge', tileValue: 2048 })
    expect(seen).toContain('game2048_reach_2048')
  })
})
