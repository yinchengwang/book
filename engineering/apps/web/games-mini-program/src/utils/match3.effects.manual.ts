// Manual test runner for match3.effects
// Run with: cd games-mini-program && npx tsx src/utils/match3.effects.manual.ts

import { createInitialState, applySpecialEffect, SpecialType } from './match3'

let pass = 0
let fail = 0
const results: { name: string; pass: boolean; error?: string }[] = []

function test(name: string, fn: () => void) {
  try {
    fn()
    pass++
    results.push({ name, pass: true })
    console.log(`  PASS: ${name}`)
  } catch (e: any) {
    fail++
    results.push({ name, pass: false, error: e.message })
    console.log(`  FAIL: ${name}: ${e.message}`)
  }
}

function assertEqual(actual: any, expected: any, msg?: string) {
  if (actual !== expected) {
    throw new Error(`${msg || 'Assertion failed'}: expected ${expected}, got ${actual}`)
  }
}

console.log('match3 特效测试')

test('横排特效清空整行', () => {
  const state = createInitialState()
  const target = 5
  applySpecialEffect(state, target, SpecialType.ROW_CLEAR)
  state.grid[target].forEach((cell, i) => {
    assertEqual(cell.type, 'empty', `row ${target} col ${i}`)
  })
})

test('纵列特效清空整列', () => {
  const state = createInitialState()
  const target = 3
  applySpecialEffect(state, target, SpecialType.COL_CLEAR)
  state.grid.forEach((row, r) => {
    assertEqual(row[target].type, 'empty', `row ${r} col ${target}`)
  })
})

test('炸弹清空 3x3 范围', () => {
  const state = createInitialState()
  applySpecialEffect(state, 4, SpecialType.BOMB)
  for (let r = 3; r <= 5; r++) {
    for (let c = 3; c <= 5; c++) {
      assertEqual(state.grid[r][c].type, 'empty', `cell (${r},${c})`)
    }
  }
})

test('彩虹特效清空所有同色', () => {
  const state = createInitialState()
  // 强制所有 cell 同色
  for (let r = 0; r < 9; r++) {
    for (let c = 0; c < 9; c++) {
      state.grid[r][c].color = 0
    }
  }
  applySpecialEffect(state, 0, SpecialType.RAINBOW)
  for (let r = 0; r < 9; r++) {
    for (let c = 0; c < 9; c++) {
      assertEqual(state.grid[r][c].type, 'empty', `cell (${r},${c})`)
    }
  }
})

console.log(`\n${pass} passed, ${fail} failed`)
if (fail > 0) process.exit(1)
