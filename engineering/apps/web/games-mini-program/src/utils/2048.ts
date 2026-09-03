/**
 * @file 2048.ts
 * @brief 2048 游戏核心逻辑（纯 JS 实现）
 *
 * PC 终端版使用 C 代码：engineering/apps/games/2048/
 * 微信小程序版使用本 JS 实现
 */

export const GAME2048_CONFIG = {
  GRID_SIZE: 4,
  WIN_VALUE: 2048
}

// 游戏状态
export interface Game2048State {
  board: number[][]       // 0=空格，非0=数字
  score: number           // 当前得分
  bestScore: number       // 历史最高分
  isGameOver: boolean     // 无可用移动
  hasWon: boolean         // 已达成 2048
  keepGoing: boolean      // 达成 2048 后继续游戏
}

/**
 * 创建空棋盘
 */
function createEmptyBoard(): number[][] {
  return Array(GAME2048_CONFIG.GRID_SIZE)
    .fill(null)
    .map(() => Array(GAME2048_CONFIG.GRID_SIZE).fill(0))
}

/**
 * 创建初始游戏状态
 */
export function createInitialState(bestScore: number = 0): Game2048State {
  const state: Game2048State = {
    board: createEmptyBoard(),
    score: 0,
    bestScore,
    isGameOver: false,
    hasWon: false,
    keepGoing: false
  }

  // 初始生成 2 个格子
  spawnTile(state)
  spawnTile(state)

  return state
}

/**
 * 深拷贝棋盘
 */
function copyBoard(board: number[][]): number[][] {
  return board.map(row => [...row])
}

/**
 * 获取所有空格位置
 */
function getEmptyCells(board: number[][]): { row: number; col: number }[] {
  const cells: { row: number; col: number }[] = []
  for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
    for (let c = 0; c < GAME2048_CONFIG.GRID_SIZE; c++) {
      if (board[r][c] === 0) {
        cells.push({ row: r, col: c })
      }
    }
  }
  return cells
}

/**
 * 生成新数字（90% 为 2，10% 为 4）
 */
export function spawnTile(state: Game2048State): boolean {
  const emptyCells = getEmptyCells(state.board)
  if (emptyCells.length === 0) return false

  const pos = emptyCells[Math.floor(Math.random() * emptyCells.length)]
  state.board[pos.row][pos.col] = Math.random() < 0.9 ? 2 : 4
  return true
}

/**
 * 压缩一行（移除空格）
 */
function compressRow(row: number[]): number[] {
  return row.filter(val => val !== 0)
}

/**
 * 合并一行（返回合并后的行和得分增量）
 */
function mergeRow(row: number[]): { row: number[]; addedScore: number } {
  const { GRID_SIZE } = GAME2048_CONFIG
  const result: number[] = []
  let addedScore = 0
  let i = 0

  while (i < row.length) {
    if (i + 1 < row.length && row[i] === row[i + 1]) {
      // 合并
      const merged = row[i] * 2
      result.push(merged)
      addedScore += merged
      i += 2
    } else {
      result.push(row[i])
      i++
    }
  }

  // 补齐到 GRID_SIZE
  while (result.length < GRID_SIZE) {
    result.push(0)
  }

  return { row: result, addedScore }
}

/**
 * 移动并合并一行
 */
function slideRow(row: number[]): { row: number[]; moved: boolean; addedScore: number } {
  const compressed = compressRow(row)
  const { row: merged, addedScore } = mergeRow(compressed)

  // 检查是否有变化
  const moved = row.some((val, i) => val !== merged[i])

  return { row: merged, moved, addedScore }
}

/**
 * 移动棋盘（通用）
 */
function moveBoard(
  state: Game2048State,
  transform: (board: number[][]) => number[][],
  reverse: boolean
): boolean {
  const originalBoard = copyBoard(state.board)
  let totalAddedScore = 0

  // 变换棋盘
  let board = transform(state.board)

  // 对每一行执行滑动
  const newBoard: number[][] = []
  for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
    let row = board[r]
    if (reverse) row = [...row].reverse()

    const result = slideRow(row)

    if (reverse) result.row.reverse()
    newBoard.push(result.row)
    totalAddedScore += result.addedScore
  }

  // 检查是否有变化
  let moved = false
  for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
    for (let c = 0; c < GAME2048_CONFIG.GRID_SIZE; c++) {
      if (originalBoard[r][c] !== newBoard[r][c]) {
        moved = true
        break
      }
    }
    if (moved) break
  }

  if (moved) {
    state.board = newBoard
    state.score += totalAddedScore
    if (state.score > state.bestScore) {
      state.bestScore = state.score
    }
    spawnTile(state)

    // 检查是否达到 2048
    if (!state.hasWon) {
      for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
        for (let c = 0; c < GAME2048_CONFIG.GRID_SIZE; c++) {
          if (state.board[r][c] === GAME2048_CONFIG.WIN_VALUE) {
            state.hasWon = true
          }
        }
      }
    }
  }

  return moved
}

/**
 * 向左移动
 */
export function moveLeft(state: Game2048State): boolean {
  return moveBoard(state, (board) => board, false)
}

/**
 * 向右移动
 */
export function moveRight(state: Game2048State): boolean {
  return moveBoard(state, (board) => board, true)
}

/**
 * 滑动并合并一列
 * @param col 原始列
 * @param toTop true=向上移动（值填在前面），false=向下移动（值填在后面）
 */
function slideColumn(col: number[], toTop: boolean): { col: number[]; addedScore: number } {
  const { GRID_SIZE } = GAME2048_CONFIG

  // 收集非零值
  const values = col.filter(val => val !== 0)

  // 合并
  const merged: number[] = []
  let addedScore = 0
  let i = 0
  while (i < values.length) {
    if (i + 1 < values.length && values[i] === values[i + 1]) {
      merged.push(values[i] * 2)
      addedScore += values[i] * 2
      i += 2
    } else {
      merged.push(values[i])
      i++
    }
  }

  // 根据方向填充结果
  const result: number[] = Array(GRID_SIZE).fill(0)
  if (toTop) {
    // 向上：值从索引 0 开始填
    for (let j = 0; j < merged.length; j++) {
      result[j] = merged[j]
    }
  } else {
    // 向下：值从最后开始填
    for (let j = 0; j < merged.length; j++) {
      result[GRID_SIZE - 1 - j] = merged[merged.length - 1 - j]
    }
  }

  return { col: result, addedScore }
}

/**
 * 向上移动
 */
export function moveUp(state: Game2048State): boolean {
  const originalBoard = copyBoard(state.board)
  let totalAddedScore = 0

  for (let c = 0; c < GAME2048_CONFIG.GRID_SIZE; c++) {
    const col: number[] = []
    for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
      col.push(state.board[r][c])
    }

    const { col: slidCol, addedScore } = slideColumn(col, true)
    totalAddedScore += addedScore

    for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
      state.board[r][c] = slidCol[r]
    }
  }

  // 检查是否有变化
  let moved = false
  for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
    for (let c = 0; c < GAME2048_CONFIG.GRID_SIZE; c++) {
      if (originalBoard[r][c] !== state.board[r][c]) {
        moved = true
        break
      }
    }
    if (moved) break
  }

  if (moved) {
    state.score += totalAddedScore
    if (state.score > state.bestScore) {
      state.bestScore = state.score
    }
    spawnTile(state)

    // 检查是否达到 2048
    if (!state.hasWon) {
      for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
        for (let c = 0; c < GAME2048_CONFIG.GRID_SIZE; c++) {
          if (state.board[r][c] === GAME2048_CONFIG.WIN_VALUE) {
            state.hasWon = true
          }
        }
      }
    }
  }

  return moved
}

/**
 * 向下移动
 */
export function moveDown(state: Game2048State): boolean {
  const originalBoard = copyBoard(state.board)
  let totalAddedScore = 0

  for (let c = 0; c < GAME2048_CONFIG.GRID_SIZE; c++) {
    const col: number[] = []
    for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
      col.push(state.board[r][c])
    }

    const { col: slidCol, addedScore } = slideColumn(col, false)
    totalAddedScore += addedScore

    for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
      state.board[r][c] = slidCol[r]
    }
  }

  // 检查是否有变化
  let moved = false
  for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
    for (let c = 0; c < GAME2048_CONFIG.GRID_SIZE; c++) {
      if (originalBoard[r][c] !== state.board[r][c]) {
        moved = true
        break
      }
    }
    if (moved) break
  }

  if (moved) {
    state.score += totalAddedScore
    if (state.score > state.bestScore) {
      state.bestScore = state.score
    }
    spawnTile(state)

    // 检查是否达到 2048
    if (!state.hasWon) {
      for (let r = 0; r < GAME2048_CONFIG.GRID_SIZE; r++) {
        for (let c = 0; c < GAME2048_CONFIG.GRID_SIZE; c++) {
          if (state.board[r][c] === GAME2048_CONFIG.WIN_VALUE) {
            state.hasWon = true
          }
        }
      }
    }
  }

  return moved
}

/**
 * 检查是否还有可用移动
 */
export function canMove(state: Game2048State): boolean {
  const { GRID_SIZE } = GAME2048_CONFIG

  // 检查是否有空格
  for (let r = 0; r < GRID_SIZE; r++) {
    for (let c = 0; c < GRID_SIZE; c++) {
      if (state.board[r][c] === 0) return true
    }
  }

  // 检查是否可以合并
  for (let r = 0; r < GRID_SIZE; r++) {
    for (let c = 0; c < GRID_SIZE; c++) {
      const val = state.board[r][c]
      // 检查右边
      if (c + 1 < GRID_SIZE && state.board[r][c + 1] === val) return true
      // 检查下边
      if (r + 1 < GRID_SIZE && state.board[r + 1][c] === val) return true
    }
  }

  return false
}

/**
 * 执行一次移动（自动检测方向）
 */
export function move(state: Game2048State, direction: 'left' | 'right' | 'up' | 'down'): boolean {
  let moved = false

  switch (direction) {
    case 'left': moved = moveLeft(state); break
    case 'right': moved = moveRight(state); break
    case 'up': moved = moveUp(state); break
    case 'down': moved = moveDown(state); break
  }

  // 检查游戏结束
  if (!canMove(state) && !state.hasWon) {
    state.isGameOver = true
  }

  return moved
}

/**
 * 继续游戏（达成 2048 后）
 */
export function keepGoing(state: Game2048State): void {
  state.keepGoing = true
}

/**
 * 获取数字对应的背景色
 */
export function getTileColor(value: number): string {
  const colors: Record<number, string> = {
    0: '#3c3a32',
    2: '#eee4da',
    4: '#ede0c8',
    8: '#f2b179',
    16: '#f59563',
    32: '#f67c5f',
    64: '#f65e3b',
    128: '#edcf72',
    256: '#edcc61',
    512: '#edc850',
    1024: '#edc53f',
    2048: '#edc22e'
  }

  // 对于更大的数字，使用渐变色
  if (value > 2048) {
    return '#3c3a32'
  }

  return colors[value] || colors[0]
}

/**
 * 获取数字对应的文字颜色
 */
export function getTextColor(value: number): string {
  return value <= 4 ? '#776e65' : '#f9f6f2'
}