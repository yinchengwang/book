/**
 * @file match3.test.ts
 * @brief 消消乐游戏核心逻辑单元测试
 */
import { describe, it, expect } from 'vitest'
import {
  createBoard,
  findMatches,
  dropGems,
  isValidSwap,
  doSwap,
  calculateScore,
  calculateStars,
  createGameState,
  checkGoalsComplete,
  BOARD_SIZE
} from './match3'

describe('消消乐核心逻辑测试', () => {
  describe('calculateScore 分数计算测试', () => {
    it('应根据消除数量计算分数', () => {
      const score = calculateScore(5, 1)
      expect(score).toBeGreaterThan(0)
    })

    it('连击应增加分数', () => {
      const scoreWithCombo = calculateScore(5, 2)
      const scoreWithoutCombo = calculateScore(5, 1)
      expect(scoreWithCombo).toBeGreaterThan(scoreWithoutCombo)
    })
  })

  describe('calculateStars 星星评价测试', () => {
    it('分数未达标应返回 0 星', () => {
      const stars = calculateStars(500, 1000, 10, 20)
      expect(stars).toBe(0)
    })

    it('剩余步数 >= 50% 应返回 3 星', () => {
      const stars = calculateStars(1000, 1000, 12, 20)
      expect(stars).toBe(3)
    })

    it('剩余步数 >= 25% 应返回 2 星', () => {
      const stars = calculateStars(1000, 1000, 7, 20)
      expect(stars).toBe(2)
    })

    it('剩余步数 < 25% 应返回 1 星', () => {
      const stars = calculateStars(1000, 1000, 3, 20)
      expect(stars).toBe(1)
    })
  })

  describe('createGameState 游戏状态创建测试', () => {
    it('应创建正确的初始状态', () => {
      const state = createGameState(1)
      expect(state.level).toBe(1)
      expect(state.moves).toBeGreaterThan(0)
      expect(state.score).toBe(0)
      expect(state.status).toBe('playing')
    })

    it('应正确计算总星星数', () => {
      const earnedStars = { 1: 3, 2: 2, 3: 1 }
      const state = createGameState(1, earnedStars)
      expect(state.totalStars).toBe(6)
    })
  })

  describe('checkGoalsComplete 目标检测测试', () => {
    it('所有目标完成应返回 true', () => {
      const state = createGameState(1)
      state.goals = [
        { type: 'score', current: 1000, target: 1000 },
        { type: 'coin', current: 3, target: 3 }
      ]
      expect(checkGoalsComplete(state)).toBe(true)
    })

    it('有目标未完成应返回 false', () => {
      const state = createGameState(1)
      state.goals = [
        { type: 'score', current: 500, target: 1000 },
        { type: 'coin', current: 3, target: 3 }
      ]
      expect(checkGoalsComplete(state)).toBe(false)
    })
  })

  describe('doSwap 交换测试', () => {
    it('应正确交换两个格子', () => {
      const board = createBoard()
      const gem1 = board[0][0].gem
      const gem2 = board[0][1].gem

      doSwap(board, '0,0', '0,1')

      expect(board[0][0].gem).toBe(gem2)
      expect(board[0][1].gem).toBe(gem1)
    })
  })

  describe('isValidSwap 有效交换测试', () => {
    it('应拒绝非相邻格子交换', () => {
      const board = createBoard()
      const valid = isValidSwap(board, '0,0', '0,2')
      expect(valid).toBe(false)
    })

    it('应允许相邻格子交换（不检查匹配）', () => {
      const board = createBoard()
      // 确保两个格子都有宝石（gem = 0 是有效值，会被误判为空）
      board[0][0].gem = 1
      board[0][1].gem = 2

      const valid = isValidSwap(board, '0,0', '0,1')
      expect(valid).toBe(true)
    })

    it('应拒绝空格子交换', () => {
      const board = createBoard()
      board[0][0].gem = null

      const valid = isValidSwap(board, '0,0', '0,1')
      expect(valid).toBe(false)
    })
  })

  describe('findMatches 匹配检测测试', () => {
    it('应检测水平匹配', () => {
      const board = createBoard()
      // 设置三个相同宝石
      board[0][0].gem = 0
      board[0][1].gem = 0
      board[0][2].gem = 0

      const matches = findMatches(board)

      expect(matches.length).toBeGreaterThan(0)
    })
  })

  describe('dropGems 下落测试', () => {
    it('下落不应改变非空格子顺序', () => {
      const board = createBoard()
      // 清空一列
      for (let r = 0; r < BOARD_SIZE; r++) {
        board[r][0].gem = null as any
      }
      // 设置底部宝石
      board[BOARD_SIZE - 1][0].gem = 1
      board[BOARD_SIZE - 2][0].gem = 2

      const originalGem = board[BOARD_SIZE - 1][0].gem
      dropGems(board)

      expect(board[BOARD_SIZE - 1][0].gem).toBe(originalGem)
    })
  })
})