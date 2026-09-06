import { describe, it, expect, vi } from 'vitest'

vi.mock('@tarojs/taro', () => ({
  default: {
    getStorageSync: vi.fn(() => undefined),
    setStorageSync: vi.fn()
  }
}))

import { buildSnakeShare, build2048Share, buildSudokuShare, buildMatch3Share } from './share'

describe('分享卡片生成', () => {
  it('贪吃蛇卡片包含分数', () => {
    const card = buildSnakeShare(120, 200)
    expect(card.title).toContain('120')
    expect(card.title).toContain('200')
    expect(card.path).toBe('/pages/snake/index')
  })

  it('2048 卡片包含分数', () => {
    const card = build2048Share(2048, 4096)
    expect(card.title).toContain('2048')
  })

  it('数独卡片包含关卡和星数', () => {
    const card = buildSudokuShare(2, 5, 3)
    expect(card.title).toContain('2')
    expect(card.title).toContain('5')
    expect(card.title).toContain('3')
  })

  it('消消乐卡片包含分数', () => {
    const card = buildMatch3Share(1, 3, 1500)
    expect(card.title).toContain('1500')
  })
})
