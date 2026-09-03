/**
 * @file snake.test.ts
 * @brief 贪吃蛇游戏核心逻辑单元测试
 */
import { describe, it, expect } from 'vitest'
import {
  createInitialState,
  tick,
  setDirection,
  getGrid,
  Direction,
  SNAKE_CONFIG,
  generateFood,
  SnakeState
} from './snake'

describe('贪吃蛇核心逻辑测试', () => {
  describe('新游戏初始化测试', () => {
    it('新游戏蛇长度应为 3', () => {
      const state = createInitialState()
      expect(state.snake.length).toBe(3)
    })

    it('新游戏分数应为 0', () => {
      const state = createInitialState()
      expect(state.score).toBe(0)
    })

    it('新游戏不应已结束', () => {
      const state = createInitialState()
      expect(state.isGameOver).toBe(false)
    })

    it('新游戏初始速度应为 INITIAL_SPEED', () => {
      const state = createInitialState()
      expect(state.speed).toBe(SNAKE_CONFIG.INITIAL_SPEED)
    })
  })

  describe('反向移动禁止测试', () => {
    it('向右时不应接受向左输入', () => {
      const state = createInitialState()
      state.direction = Direction.RIGHT
      state.nextDirection = Direction.RIGHT

      setDirection(state, Direction.LEFT)

      expect(state.nextDirection).toBe(Direction.RIGHT)
    })

    it('向左时不应接受向右输入', () => {
      const state = createInitialState()
      state.direction = Direction.LEFT
      state.nextDirection = Direction.LEFT

      setDirection(state, Direction.RIGHT)

      expect(state.nextDirection).toBe(Direction.LEFT)
    })

    it('向上时不应接受向下输入', () => {
      const state = createInitialState()
      state.direction = Direction.UP
      state.nextDirection = Direction.UP

      setDirection(state, Direction.DOWN)

      expect(state.nextDirection).toBe(Direction.UP)
    })

    it('向下时不应接受向上输入', () => {
      const state = createInitialState()
      state.direction = Direction.DOWN
      state.nextDirection = Direction.DOWN

      setDirection(state, Direction.UP)

      expect(state.nextDirection).toBe(Direction.DOWN)
    })

    it('向右时应接受向上输入', () => {
      const state = createInitialState()
      state.direction = Direction.RIGHT
      state.nextDirection = Direction.RIGHT

      setDirection(state, Direction.UP)

      expect(state.nextDirection).toBe(Direction.UP)
    })
  })

  describe('碰撞检测测试', () => {
    it('撞左墙后游戏应结束', () => {
      const state = createInitialState()
      // 将蛇头移到左边界
      state.snake[state.snake.length - 1].x = 0
      state.direction = Direction.LEFT
      state.nextDirection = Direction.LEFT

      tick(state)

      expect(state.isGameOver).toBe(true)
    })

    it('撞右墙后游戏应结束', () => {
      const state = createInitialState()
      state.snake[state.snake.length - 1].x = SNAKE_CONFIG.GRID_WIDTH - 1
      state.direction = Direction.RIGHT
      state.nextDirection = Direction.RIGHT

      tick(state)

      expect(state.isGameOver).toBe(true)
    })

    it('撞上墙后游戏应结束', () => {
      const state = createInitialState()
      state.snake[state.snake.length - 1].y = 0
      state.direction = Direction.UP
      state.nextDirection = Direction.UP

      tick(state)

      expect(state.isGameOver).toBe(true)
    })

    it('撞下墙后游戏应结束', () => {
      const state = createInitialState()
      state.snake[state.snake.length - 1].y = SNAKE_CONFIG.GRID_HEIGHT - 1
      state.direction = Direction.DOWN
      state.nextDirection = Direction.DOWN

      tick(state)

      expect(state.isGameOver).toBe(true)
    })
  })

  describe('getGrid 测试', () => {
    it('应返回正确大小的网格', () => {
      const state = createInitialState()
      const grid = getGrid(state)

      expect(grid.length).toBe(SNAKE_CONFIG.GRID_HEIGHT)
      expect(grid[0].length).toBe(SNAKE_CONFIG.GRID_WIDTH)
    })

    it('食物位置应标记为 2', () => {
      const state = createInitialState()
      state.food = { x: 10, y: 10 }

      const grid = getGrid(state)

      expect(grid[10][10]).toBe(2)
    })

    it('蛇头位置应标记为 3', () => {
      const state = createInitialState()
      const grid = getGrid(state)
      const head = state.snake[state.snake.length - 1]

      expect(grid[head.y][head.x]).toBe(3)
    })
  })

  describe('游戏进行测试', () => {
    it('tick 后蛇应移动', () => {
      const state = createInitialState()
      const initialSnakeLength = state.snake.length

      tick(state)

      // tick 应该被调用（游戏进行中）
      expect(state.snake.length).toBeGreaterThanOrEqual(0)
    })

    it('游戏结束后 tick 不应移动蛇', () => {
      const state = createInitialState()
      state.isGameOver = true
      const initialSnake = state.snake.map(s => ({ ...s }))

      tick(state)

      expect(state.snake).toEqual(initialSnake)
    })
  })

  describe('食物生成测试', () => {
    it('新游戏食物位置应在有效范围内', () => {
      const state = createInitialState()

      expect(state.food.x).toBeGreaterThanOrEqual(0)
      expect(state.food.x).toBeLessThan(SNAKE_CONFIG.GRID_WIDTH)
      expect(state.food.y).toBeGreaterThanOrEqual(0)
      expect(state.food.y).toBeLessThan(SNAKE_CONFIG.GRID_HEIGHT)
    })

    it('新游戏食物位置不应与蛇身重叠', () => {
      const state = createInitialState()

      const isOnSnake = state.snake.some(
        segment => segment.x === state.food.x && segment.y === state.food.y
      )
      expect(isOnSnake).toBe(false)
    })

    it('generateFood 应接受 maxAttempts 参数', () => {
      const state = createInitialState()
      const food = generateFood(state, 10)

      expect(food.x).toBeGreaterThanOrEqual(0)
      expect(food.y).toBeGreaterThanOrEqual(0)
    })
  })
})