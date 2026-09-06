import { describe, it, expect } from 'vitest'
import { buildSnakeShare, build2048Share, buildSudokuShare, buildMatch3Share } from './share'

/**
 * @brief share.ts 是纯函数模块，不依赖 Taro；无需 mock。
 * 测试覆盖 4 个 builder 的 title 字段填充 + path 路由正确性。
 */

describe('分享卡片生成', () => {
  it('贪吃蛇卡片包含分数和最高分', () => {
    const card = buildSnakeShare(120, 200)
    expect(card.title).toContain('120')
    expect(card.title).toContain('200')
    expect(card.path).toBe('/pages/snake/index')
  })

  it('2048 卡片包含分数和最高分', () => {
    // 用不与模板前缀 '2048' 冲突的输入，避免 toContain('2048') 的假阳性
    const card = build2048Share(1234, 5678)
    expect(card.title).toContain('1234')
    expect(card.title).toContain('5678')
    expect(card.path).toBe('/pages/game2048/index')
  })

  it('数独卡片包含章节、关卡、星数', () => {
    // 多位数输入避免单字符 toContain 误匹配
    const card = buildSudokuShare(10, 25, 3)
    expect(card.title).toContain('10')
    expect(card.title).toContain('25')
    expect(card.title).toContain('3')
    expect(card.path).toBe('/pages/sudoku/index')
  })

  it('消消乐卡片包含章节、关卡、分数', () => {
    const card = buildMatch3Share(7, 12, 1500)
    expect(card.title).toContain('7')
    expect(card.title).toContain('12')
    expect(card.title).toContain('1500')
    expect(card.path).toBe('/pages/match3/index')
  })

  it('零分卡片仍然返回合法结构', () => {
    const card = buildSnakeShare(0, 0)
    expect(card.title).toBeTruthy()
    expect(card.path).toBe('/pages/snake/index')
  })
})