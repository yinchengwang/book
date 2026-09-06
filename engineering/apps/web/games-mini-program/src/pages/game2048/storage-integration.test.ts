import { describe, it, expect, beforeEach, vi } from 'vitest'

vi.mock('@tarojs/taro', () => ({
  default: {
    getStorageSync: vi.fn(() => undefined),
    setStorageSync: vi.fn()
  }
}))

import { get2048BestScore, update2048BestScore } from '@/utils/storage'

describe('game2048 storage integration', () => {
  beforeEach(() => update2048BestScore(0))

  it('新最高分被持久化', () => {
    update2048BestScore(2048)
    expect(get2048BestScore()).toBe(2048)
  })

  it('低于最高分的提交不会覆盖', () => {
    update2048BestScore(4096)
    update2048BestScore(2048)
    expect(get2048BestScore()).toBe(4096)
  })
})
