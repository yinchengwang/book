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

import {
  getMatch3Data,
  unlockMatch3Chapter,
  setMatch3ChapterStars
} from '@/utils/storage'

describe('match3 章节进度', () => {
  it('首次解锁第 1 章', () => {
    unlockMatch3Chapter(1)
    expect(getMatch3Data().chapters[1].unlocked).toBe(true)
  })

  it('记录星星数', () => {
    setMatch3ChapterStars(1, 1, 2)
    expect(getMatch3Data().chapters[1].stars[1]).toBe(2)
  })
})