/**
 * @file 2048.test.ts
 * @brief 2048 游戏核心逻辑单元测试
 */
import { describe, it, expect, beforeEach } from 'vitest'
import {
  createInitialState,
  moveLeft,
  moveRight,
  moveUp,
  moveDown,
  canMove,
  GAME2048_CONFIG,
  Game2048State
} from './2048'

// 辅助函数：创建带有指定棋盘的初始状态
function createStateWithBoard(board: number[][]): Game2048State {
  const state: Game2048State = {
    board: board.map(row => [...row]),
    score: 0,
    bestScore: 0,
    isGameOver: false,
    hasWon: false,
    keepGoing: false
  }
  return state
}

describe('2048 核心逻辑测试', () => {
  describe('向左移动合并测试', () => {
    it('应合并两个相同的数字', () => {
      const state = createStateWithBoard([
        [2, 2, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0]
      ])

      const moved = moveLeft(state)

      expect(moved).toBe(true)
      // 合并后的结果应该包含 4
      expect(state.board[0].includes(4)).toBe(true)
      expect(state.score).toBe(4)
    })

    it('应合并连续相同的数字', () => {
      const state = createStateWithBoard([
        [2, 2, 2, 2],
        [0, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0]
      ])

      moveLeft(state)

      expect(state.board[0]).toEqual([4, 4, 0, 0])
      expect(state.score).toBe(8)
    })

    it('压缩后相邻的数字应合并', () => {
      // [2,0,2,0] 向左移动时会先压缩为 [2,2,0,0]，再合并为 [4,0,0,0]
      const state = createStateWithBoard([
        [2, 0, 2, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0]
      ])

      moveLeft(state)

      // 压缩后 [2,2,0,0] -> 合并 [4,0,0,0]
      expect(state.board[0][0]).toBe(4)
      expect(state.board[0][1]).toBe(0)
      expect(state.score).toBe(4)
    })

    it('每个数字只应合并一次', () => {
      const state = createStateWithBoard([
        [4, 4, 4, 4],
        [0, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0]
      ])

      moveLeft(state)

      // [4,4,4,4] -> 压缩 [4,4,4,4] -> 合并 [8,8] -> [8,8,0,0]
      // 但 spawnTile 会添加一个新块，所以检查前两个位置
      expect(state.board[0][0]).toBe(8)
      expect(state.board[0][1]).toBe(8)
      expect(state.score).toBe(16)
    })
  })

  describe('向右移动合并测试', () => {
    it('应向右合并相同数字', () => {
      const state = createStateWithBoard([
        [0, 0, 2, 2],
        [0, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0]
      ])

      const moved = moveRight(state)

      expect(moved).toBe(true)
      // 合并后的结果应该包含 4
      expect(state.board[0].includes(4)).toBe(true)
      expect(state.score).toBe(4)
    })
  })

  describe('向上移动合并测试', () => {
    it('应向上合并相同数字', () => {
      const state = createStateWithBoard([
        [2, 0, 0, 0],
        [2, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0]
      ])

      const moved = moveUp(state)

      expect(moved).toBe(true)
      // 合并后的结果应该包含 4
      expect(state.board[0][0]).toBe(4)
      expect(state.score).toBe(4)
    })

    it('应将列中的数字向上移动到顶部', () => {
      const state = createStateWithBoard([
        [0, 0, 0, 0],
        [0, 0, 0, 2],
        [0, 0, 0, 4],
        [0, 0, 0, 4]
      ])

      moveUp(state)

      // 列3: [0,2,4,4] → [2,8,0,0]
      expect(state.board[0][3]).toBe(2)
      expect(state.board[1][3]).toBe(8)
      expect(state.board[2][3]).toBe(0)
      expect(state.board[3][3]).toBe(0)
    })

    it('不同列应独立处理', () => {
      const state = createStateWithBoard([
        [2, 0, 0, 0],
        [0, 2, 0, 0],
        [0, 0, 4, 0],
        [0, 0, 0, 4]
      ])

      moveUp(state)

      // 每列独立移动和合并
      expect(state.board[0][0]).toBe(2)
      expect(state.board[0][1]).toBe(2)
      expect(state.board[0][2]).toBe(4)
      expect(state.board[0][3]).toBe(4)
    })
  })

  describe('向下移动合并测试', () => {
    it('应向下合并相同数字', () => {
      const state = createStateWithBoard([
        [2, 0, 0, 0],
        [2, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0]
      ])

      const moved = moveDown(state)

      expect(moved).toBe(true)
      // 合并后应该有分数增加
      expect(state.score).toBe(4)
    })

    it('应将列中的数字向下移动到底部', () => {
      const state = createStateWithBoard([
        [0, 0, 0, 0],
        [0, 0, 0, 2],
        [0, 0, 0, 4],
        [0, 0, 0, 4]
      ])

      moveDown(state)

      // 列3: [0,2,4,4] → [0,0,2,8]
      expect(state.board[0][3]).toBe(0)
      expect(state.board[1][3]).toBe(0)
      expect(state.board[2][3]).toBe(2)
      expect(state.board[3][3]).toBe(8)
    })

    it('不同列应独立处理', () => {
      const state = createStateWithBoard([
        [2, 0, 0, 0],
        [0, 2, 0, 0],
        [0, 0, 4, 0],
        [0, 0, 0, 4]
      ])

      moveDown(state)

      // 每列独立移动和合并
      expect(state.board[3][0]).toBe(2)
      expect(state.board[3][1]).toBe(2)
      expect(state.board[3][2]).toBe(4)
      expect(state.board[3][3]).toBe(4)
    })
  })

  describe('游戏结束检测测试', () => {
    it('应检测到棋盘已满且无合并可能', () => {
      const state = createStateWithBoard([
        [2, 4, 2, 4],
        [4, 2, 4, 2],
        [2, 4, 2, 4],
        [4, 2, 4, 2]
      ])

      expect(canMove(state)).toBe(false)
    })

    it('应检测到仍有空格', () => {
      const state = createStateWithBoard([
        [2, 4, 2, 4],
        [4, 2, 0, 2],
        [2, 4, 2, 4],
        [4, 2, 4, 2]
      ])

      expect(canMove(state)).toBe(true)
    })

    it('应检测到水平方向可合并', () => {
      const state = createStateWithBoard([
        [2, 2, 4, 2],
        [4, 2, 4, 2],
        [2, 4, 2, 4],
        [4, 2, 4, 2]
      ])

      expect(canMove(state)).toBe(true)
    })

    it('应检测到垂直方向可合并', () => {
      const state = createStateWithBoard([
        [2, 4, 2, 4],
        [2, 2, 4, 2],
        [2, 4, 2, 4],
        [4, 2, 4, 2]
      ])

      expect(canMove(state)).toBe(true)
    })
  })

  describe('胜利条件检测测试', () => {
    it('应在棋盘出现 2048 时标记 hasWon', () => {
      const state = createStateWithBoard([
        [1024, 1024, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0]
      ])

      moveLeft(state)

      expect(state.board[0][0]).toBe(2048)
      expect(state.hasWon).toBe(true)
    })
  })

  describe('无效移动测试', () => {
    it('棋盘无变化时不应生成新块', () => {
      const state = createStateWithBoard([
        [2, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0],
        [0, 0, 0, 0]
      ])

      const initialNonZero = state.board.flat().filter(v => v !== 0).length
      moveLeft(state) // 已经是最左边了，无变化
      const afterNonZero = state.board.flat().filter(v => v !== 0).length

      expect(afterNonZero).toBe(initialNonZero)
    })
  })

  describe('初始状态测试', () => {
    it('新游戏应有 2 个初始块', () => {
      const state = createInitialState(0)
      const nonZeroCount = state.board.flat().filter(v => v !== 0).length
      expect(nonZeroCount).toBe(2)
    })

    it('新游戏分数应为 0', () => {
      const state = createInitialState(0)
      expect(state.score).toBe(0)
    })
  })
})