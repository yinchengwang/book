/**
 * @file storage.test.ts
 * @brief 持久化层单测（使用 vitest mock 替换 Taro 存储）
 */
import { describe, it, expect, beforeEach, vi } from 'vitest'

// Expose in-memory store so beforeEach can wipe it between tests
const __taroStore: Record<string, string> = {}

// Mock Taro before importing storage
vi.mock('@tarojs/taro', () => {
  return {
    default: {
      getStorageSync: vi.fn((key: string) => __taroStore[key]),
      setStorageSync: vi.fn((key: string, value: string) => {
        __taroStore[key] = value
      }),
      removeStorageSync: vi.fn((key: string) => {
        delete __taroStore[key]
      })
    }
  }
})

import {
  getSnakeBestScore,
  updateSnakeBestScore,
  getAchievements,
  recordAchievementProgress
} from './storage'

describe('storage 层', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    // Wipe in-memory mock store between tests so each test starts fresh
    Object.keys(__taroStore).forEach(k => delete __taroStore[k])
  })

  it('updateSnakeBestScore 写入新最高分', () => {
    updateSnakeBestScore(50)
    expect(getSnakeBestScore()).toBe(50)
  })

  it('updateSnakeBestScore 不会降低已有最高分', () => {
    updateSnakeBestScore(100)
    updateSnakeBestScore(40)
    expect(getSnakeBestScore()).toBe(100)
  })

  it('recordAchievementProgress 累计增量', () => {
    recordAchievementProgress('snake_score_100', 30)
    recordAchievementProgress('snake_score_100', 80)
    const progress = getAchievements().progress
    expect(progress.snake_score_100).toBe(110)
  })

  it('recordAchievementProgress 不会回退（负 delta）', () => {
    recordAchievementProgress('match3_three_stars', 50)
    recordAchievementProgress('match3_three_stars', -10) // 负 delta 应被夹紧
    const progress = getAchievements().progress
    expect(progress.match3_three_stars).toBe(50)
  })
})
