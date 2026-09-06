import { describe, it, expect } from 'vitest'
import { createInitialState, applySpecialEffect, SpecialType } from './match3'

describe('match3 特效', () => {
  it('横排特效清空整行', () => {
    const state = createInitialState()
    const target = 5
    applySpecialEffect(state, target, SpecialType.ROW_CLEAR)
    state.grid[target].forEach(cell => expect(cell.type).toBe('empty'))
  })

  it('纵列特效清空整列', () => {
    const state = createInitialState()
    const target = 3
    applySpecialEffect(state, target, SpecialType.COL_CLEAR)
    state.grid.forEach(row => expect(row[target].type).toBe('empty'))
  })

  it('炸弹清空 3x3 范围', () => {
    const state = createInitialState()
    applySpecialEffect(state, 4, SpecialType.BOMB)
    for (let r = 3; r <= 5; r++) {
      for (let c = 3; c <= 5; c++) {
        expect(state.grid[r][c].type).toBe('empty')
      }
    }
  })
})
