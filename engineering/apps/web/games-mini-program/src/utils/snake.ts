/**
 * @file snake.ts
 * @brief 贪吃蛇游戏核心逻辑（纯 JS 实现）
 *
 * PC 终端版使用 C 代码：engineering/apps/games/snake/
 * 微信小程序版使用本 JS 实现
 */

// 游戏配置
export const SNAKE_CONFIG = {
  GRID_WIDTH: 20,
  GRID_HEIGHT: 15,
  INITIAL_SPEED: 150,  // 毫秒
  SPEED_INCREMENT: 2,  // 每吃一个食物减少的毫秒数
  MIN_SPEED: 50        // 最小速度（最快）
}

// 方向枚举
export enum Direction {
  UP = 0,
  RIGHT = 1,
  DOWN = 2,
  LEFT = 3
}

// 位置
export interface Position {
  x: number
  y: number
}

// 游戏状态
export interface SnakeState {
  snake: Position[]      // 蛇身（头在最后）
  food: Position         // 食物位置
  direction: Direction   // 当前方向
  nextDirection: Direction // 下一帧方向
  score: number          // 得分
  isGameOver: boolean    // 游戏结束
  speed: number          // 当前速度
}

// 方向向量
const DIR_VECTORS: Record<Direction, Position> = {
  [Direction.UP]:    { x: 0, y: -1 },
  [Direction.DOWN]:  { x: 0, y: 1 },
  [Direction.LEFT]:  { x: -1, y: 0 },
  [Direction.RIGHT]: { x: 1, y: 0 }
}

// 相反方向检测
const OPPOSITE_DIR: Record<Direction, Direction> = {
  [Direction.UP]: Direction.DOWN,
  [Direction.DOWN]: Direction.UP,
  [Direction.LEFT]: Direction.RIGHT,
  [Direction.RIGHT]: Direction.LEFT
}

/**
 * 创建初始游戏状态
 */
export function createInitialState(): SnakeState {
  const centerX = Math.floor(SNAKE_CONFIG.GRID_WIDTH / 2)
  const centerY = Math.floor(SNAKE_CONFIG.GRID_HEIGHT / 2)

  const snake: Position[] = [
    { x: centerX, y: centerY },
    { x: centerX - 1, y: centerY },
    { x: centerX - 2, y: centerY }
  ]

  // 创建临时状态用于生成食物
  const tempState: SnakeState = {
    snake,
    food: { x: 0, y: 0 },
    direction: Direction.RIGHT,
    nextDirection: Direction.RIGHT,
    score: 0,
    isGameOver: false,
    speed: SNAKE_CONFIG.INITIAL_SPEED
  }

  // 随机生成食物位置
  tempState.food = generateFood(tempState)

  return tempState
}

/**
 * 生成新食物位置
 * @param state 游戏状态
 * @param maxAttempts 最大尝试次数，防止死循环
 */
export function generateFood(state: SnakeState, maxAttempts: number = 100): Position {
  const { GRID_WIDTH, GRID_HEIGHT } = SNAKE_CONFIG
  let newFood: Position
  let attempts = 0

  do {
    newFood = {
      x: Math.floor(Math.random() * GRID_WIDTH),
      y: Math.floor(Math.random() * GRID_HEIGHT)
    }
    attempts++
  } while (isSnakePosition(state.snake, newFood) && attempts < maxAttempts)

  return newFood
}

/**
 * 检查位置是否为蛇身
 */
function isSnakePosition(snake: Position[], pos: Position): boolean {
  return snake.some(segment => segment.x === pos.x && segment.y === pos.y)
}

/**
 * 设置方向（带反向检测）
 */
export function setDirection(state: SnakeState, newDir: Direction): void {
  // 防止反向移动
  if (newDir !== OPPOSITE_DIR[state.direction]) {
    state.nextDirection = newDir
  }
}

/**
 * 执行一帧游戏逻辑
 */
export function tick(state: SnakeState): void {
  if (state.isGameOver) return

  const { GRID_WIDTH, GRID_HEIGHT } = SNAKE_CONFIG

  // 应用方向
  state.direction = state.nextDirection

  // 计算新头部位置
  const head = state.snake[state.snake.length - 1]
  const dir = DIR_VECTORS[state.direction]
  const newHead: Position = {
    x: head.x + dir.x,
    y: head.y + dir.y
  }

  // 碰撞检测：墙壁
  if (newHead.x < 0 || newHead.x >= GRID_WIDTH ||
      newHead.y < 0 || newHead.y >= GRID_HEIGHT) {
    state.isGameOver = true
    return
  }

  // 碰撞检测：自身
  if (isSnakePosition(state.snake.slice(0, -1), newHead)) {
    state.isGameOver = true
    return
  }

  // 移动蛇
  state.snake.push(newHead)

  // 检查是否吃到食物
  if (newHead.x === state.food.x && newHead.y === state.food.y) {
    // 加分（每吃一个食物 +10）
    state.score += 10

    // 生成新食物
    state.food = generateFood(state)

    // 加速
    state.speed = Math.max(
      SNAKE_CONFIG.MIN_SPEED,
      state.speed - SNAKE_CONFIG.SPEED_INCREMENT
    )
  } else {
    // 没吃到食物，尾巴不移动
    state.snake.shift()
  }
}

/**
 * 获取棋盘网格状态（用于渲染）
 * 返回二维数组：0=空格，1=蛇身，2=食物，3=蛇头
 */
export function getGrid(state: SnakeState): number[][] {
  const { GRID_WIDTH, GRID_HEIGHT } = SNAKE_CONFIG
  const grid: number[][] = Array(GRID_HEIGHT)
    .fill(null)
    .map(() => Array(GRID_WIDTH).fill(0))

  // 标记食物
  grid[state.food.y][state.food.x] = 2

  // 标记蛇身
  const head = state.snake[state.snake.length - 1]
  state.snake.forEach((segment, index) => {
    grid[segment.y][segment.x] = index === state.snake.length - 1 ? 3 : 1
  })

  return grid
}

/**
 * 获取当前方向对应的旋转角度（用于渲染蛇头）
 */
export function getHeadRotation(direction: Direction): number {
  switch (direction) {
    case Direction.UP: return 0
    case Direction.RIGHT: return 90
    case Direction.DOWN: return 180
    case Direction.LEFT: return 270
  }
}