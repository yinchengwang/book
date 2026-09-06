import { describe, it, expect, beforeEach, vi } from 'vitest'

// vi.hoisted 把 store 提前到 import 之上，避免 mock 工厂 TDZ
const { store } = vi.hoisted(() => {
  const store: Record<string, string> = {}
  return { store }
})

vi.mock('@tarojs/taro', () => ({
  default: {
    getStorageSync: vi.fn((k: string) => store[k]),
    setStorageSync: vi.fn((k: string, v: string) => { store[k] = v }),
    removeStorageSync: vi.fn((k: string) => { delete store[k] })
  }
}))

import {
  getMatch3Data,
  unlockMatch3Chapter,
  setMatch3ChapterStars
} from '@/utils/storage'

/**
 * @brief match3 章节进度持久化：覆盖 set/get/unlock 三个核心路径。
 * 注意：测试间清空 store，避免顺序依赖。
 */
describe('match3 章节进度', () => {
  beforeEach(() => {
    Object.keys(store).forEach(k => delete store[k])
  })

  it('首次解锁第 1 章', () => {
    unlockMatch3Chapter(1)
    expect(getMatch3Data().chapters[1].unlocked).toBe(true)
  })

  it('记录星星数', () => {
    setMatch3ChapterStars(1, 1, 2)
    expect(getMatch3Data().chapters[1].stars[1]).toBe(2)
  })
})