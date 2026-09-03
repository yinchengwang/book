/**
 * @file sudoku.test.ts
 * @brief 数独游戏核心逻辑单元测试
 */
import { describe, it, expect } from 'vitest'
import {
  createInitialState,
  placeNumber,
  eraseCell,
  hint,
  moveCursor,
  setCursor,
  selectNumber,
  updateConflicts,
  checkWin,
  SudokuState
} from './sudoku'

describe('数独核心逻辑测试', () => {
  describe('新游戏初始化测试', () => {
    it('新游戏提示次数应为 3', () => {
      const state = createInitialState(0)
      expect(state.hintCount).toBe(3)
    })

    it('新游戏应不是游戏结束状态', () => {
      const state = createInitialState(0)
      expect(state.isGameOver).toBe(false)
    })

    it('新游戏应选中数字 1', () => {
      const state = createInitialState(0)
      expect(state.selectedNumber).toBe(1)
    })
  })

  describe('光标移动测试', () => {
    it('moveCursor 应正确移动光标', () => {
      const state = createInitialState(0)
      state.cursorRow = 5
      state.cursorCol = 5

      moveCursor(state, -1, 0) // 向左

      expect(state.cursorCol).toBe(4)
    })

    it('moveCursor 应循环边界', () => {
      const state = createInitialState(0)
      state.cursorRow = 0
      state.cursorCol = 0

      moveCursor(state, -1, 0) // 向左越界

      expect(state.cursorCol).toBe(8)
    })

    it('setCursor 应设置光标位置', () => {
      const state = createInitialState(0)

      setCursor(state, 3, 4)

      expect(state.cursorRow).toBe(3)
      expect(state.cursorCol).toBe(4)
    })

    it('setCursor 应限制边界', () => {
      const state = createInitialState(0)

      setCursor(state, -1, 10)

      expect(state.cursorRow).toBe(0)
      expect(state.cursorCol).toBe(8)
    })
  })

  describe('数字选择测试', () => {
    it('selectNumber 应设置选中的数字', () => {
      const state = createInitialState(0)

      selectNumber(state, 7)

      expect(state.selectedNumber).toBe(7)
    })

    it('selectNumber 应限制在 1-9 范围', () => {
      const state = createInitialState(0)

      selectNumber(state, 15)

      expect(state.selectedNumber).toBe(9)
    })

    it('selectNumber 应限制最小值', () => {
      const state = createInitialState(0)

      selectNumber(state, -3)

      expect(state.selectedNumber).toBe(1)
    })
  })

  describe('提示功能测试', () => {
    it('提示次数耗尽应返回 false', () => {
      const state = createInitialState(0)
      state.hintCount = 0

      const success = hint(state)

      expect(success).toBe(false)
    })

    it('无空格时提示应返回 false', () => {
      const state = createInitialState(0)
      // 填满所有格子
      for (let r = 0; r < 9; r++) {
        for (let c = 0; c < 9; c++) {
          state.board[r][c].value = 5
        }
      }
      state.hintCount = 3

      const success = hint(state)

      expect(success).toBe(false)
      expect(state.hintCount).toBe(3)
    })

    it('有空格时提示应返回 true', () => {
      const state = createInitialState(0)
      state.hintCount = 3
      // 确保有空格
      state.board[0][0].value = 0

      const success = hint(state)

      expect(success).toBe(true)
      expect(state.hintCount).toBe(2)
    })
  })

  describe('游戏结束检测测试', () => {
    it('棋盘填满时应标记游戏结束', () => {
      const state = createInitialState(0)
      // 填满所有格子
      for (let r = 0; r < 9; r++) {
        for (let c = 0; c < 9; c++) {
          state.board[r][c].value = 5
        }
      }
      state.isGameOver = false

      checkWin(state)

      expect(state.isGameOver).toBe(true)
    })

    it('有空格时不应标记游戏结束', () => {
      const state = createInitialState(0)
      state.board[0][0].value = 0
      state.isGameOver = false

      checkWin(state)

      expect(state.isGameOver).toBe(false)
    })
  })

  describe('擦除功能测试', () => {
    it('应能擦除非固定格', () => {
      const state = createInitialState(0)
      state.cursorRow = 0
      state.cursorCol = 0
      state.board[0][0].value = 5
      state.board[0][0].isGiven = false

      eraseCell(state)

      expect(state.board[0][0].value).toBe(0)
    })
  })
})