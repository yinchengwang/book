import { describe, it, expect, beforeEach, vi } from 'vitest'

vi.mock('@tarojs/taro', () => {
  const store: Record<string, string> = {}
  return {
    default: {
      getStorageSync: vi.fn((key: string) => store[key]),
      setStorageSync: vi.fn((key: string, value: string) => { store[key] = value }),
      removeStorageSync: vi.fn((key: string) => { delete store[key] })
    }
  }
})

import {
  getSudokuData,
  setSudokuData,
  unlockSudokuChapter,
  setSudokuChapterStars
} from '@/utils/storage'

describe('sudoku 章节进度', () => {
  beforeEach(() => {
    setSudokuData({
      bestScore: 0,
      chapters: [
        { id: 1, unlocked: true, stars: {} },
        { id: 2, unlocked: false, stars: {} },
        { id: 3, unlocked: false, stars: {} },
        { id: 4, unlocked: false, stars: {} },
        { id: 5, unlocked: false, stars: {} }
      ]
    })
  })

  it('默认第一章解锁', () => {
    expect(getSudokuData().chapters[0].unlocked).toBe(true)
  })

  it('解锁第二章', () => {
    unlockSudokuChapter(2)
    expect(getSudokuData().chapters[1].unlocked).toBe(true)
  })

  it('记录星星数', () => {
    setSudokuChapterStars(1, 1, 3)
    expect(getSudokuData().chapters[0].stars[1]).toBe(3)
  })
})
