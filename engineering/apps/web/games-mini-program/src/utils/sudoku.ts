/**
 * @file sudoku.ts
 * @brief 数独游戏核心逻辑（纯 JS 实现）
 *
 * PC 终端版使用 C 代码：engineering/apps/games/sudoku/
 * 微信小程序版使用本 JS 实现
 */

export const SUDOKU_CONFIG = {
  GRID_SIZE: 9,
  BOX_SIZE: 3,
  DIFFICULTIES: [
    { name: '简单', holes: 30, maxTime: 180, description: '入门' },
    { name: '中等', holes: 40, maxTime: 300, description: '进阶' },
    { name: '困难', holes: 50, maxTime: 600, description: '挑战' }
  ]
}

// 单元格
export interface Cell {
  value: number      // 0=空, 1-9=数字
  isGiven: boolean   // 是否为题目固定格
  isConflict: boolean // 是否冲突
}

// 游戏状态
export interface SudokuState {
  board: Cell[][]           // 当前棋盘
  solution: number[][]      // 完整解
  cursorRow: number         // 光标行 (0-8)
  cursorCol: number         // 光标列 (0-8)
  difficulty: number        // 难度等级 (0-2)
  emptyCount: number        // 剩余空格数
  isGameOver: boolean       // 是否完成
  selectedNumber: number    // 当前选中的数字 (1-9)
  hintCount: number         // 剩余提示次数
}

/**
 * 创建空棋盘
 */
function createEmptyBoard(): Cell[][] {
  return Array(SUDOKU_CONFIG.GRID_SIZE)
    .fill(null)
    .map(() =>
      Array(SUDOKU_CONFIG.GRID_SIZE)
        .fill(null)
        .map(() => ({
          value: 0,
          isGiven: false,
          isConflict: false
        }))
    )
}

/**
 * 深拷贝棋盘
 */
function copyBoard(board: Cell[][]): Cell[][] {
  return board.map(row => row.map(cell => ({ ...cell })))
}

/**
 * 深拷贝解
 */
function copySolution(solution: number[][]): number[][] {
  return solution.map(row => [...row])
}

/**
 * 检查在某位置放置数字是否有效
 */
function isValidPlacement(
  board: Cell[][],
  row: number,
  col: number,
  num: number
): boolean {
  const { GRID_SIZE, BOX_SIZE } = SUDOKU_CONFIG

  // 检查行
  for (let c = 0; c < GRID_SIZE; c++) {
    if (c !== col && board[row][c].value === num) return false
  }

  // 检查列
  for (let r = 0; r < GRID_SIZE; r++) {
    if (r !== row && board[r][col].value === num) return false
  }

  // 检查 3x3 宫格
  const boxRow = Math.floor(row / BOX_SIZE) * BOX_SIZE
  const boxCol = Math.floor(col / BOX_SIZE) * BOX_SIZE
  for (let r = boxRow; r < boxRow + BOX_SIZE; r++) {
    for (let c = boxCol; c < boxCol + BOX_SIZE; c++) {
      if (r !== row && c !== col && board[r][c].value === num) return false
    }
  }

  return true
}

/**
 * 求解数独（回溯算法）
 */
function solveSudoku(board: Cell[][]): boolean {
  const { GRID_SIZE } = SUDOKU_CONFIG

  for (let row = 0; row < GRID_SIZE; row++) {
    for (let col = 0; col < GRID_SIZE; col++) {
      if (board[row][col].value === 0) {
        for (let num = 1; num <= GRID_SIZE; num++) {
          if (isValidPlacement(board, row, col, num)) {
            board[row][col].value = num
            if (solveSudoku(board)) return true
            board[row][col].value = 0
          }
        }
        return false
      }
    }
  }
  return true
}

/**
 * 统计解的数量（最多统计到 limit）
 */
function countSolutions(
  board: Cell[][],
  solution: number[][],
  limit: number = 2
): number {
  const { GRID_SIZE } = SUDOKU_CONFIG
  let count = 0

  // 将 board 复制到临时棋盘
  const tempBoard = copyBoard(board)

  for (let row = 0; row < GRID_SIZE; row++) {
    for (let col = 0; col < GRID_SIZE; col++) {
      if (tempBoard[row][col].value === 0) {
        for (let num = 1; num <= GRID_SIZE; num++) {
          if (isValidPlacement(tempBoard, row, col, num)) {
            tempBoard[row][col].value = num
            count += countSolutions(tempBoard, solution, limit - count)
            tempBoard[row][col].value = 0
            if (count >= limit) return count
          }
        }
        return count
      }
    }
  }

  // 找到一个解
  if (count === 0) {
    // 保存解
    for (let r = 0; r < GRID_SIZE; r++) {
      for (let c = 0; c < GRID_SIZE; c++) {
        solution[r][c] = tempBoard[r][c].value
      }
    }
    return 1
  }

  return count
}

/**
 * 生成完整数独解
 */
function generateFullBoard(): number[][] {
  const board = createEmptyBoard()
  solveSudoku(board)
  return board.map(row => row.map(cell => cell.value))
}

/**
 * 移除数字形成题目
 */
function removeNumbers(
  solution: number[][],
  board: Cell[][],
  holes: number
): number {
  const { GRID_SIZE } = SUDOKU_CONFIG
  const positions: { row: number; col: number }[] = []

  // 生成所有位置
  for (let r = 0; r < GRID_SIZE; r++) {
    for (let c = 0; c < GRID_SIZE; c++) {
      positions.push({ row: r, col: c })
    }
  }

  // 随机打乱
  for (let i = positions.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1))
    ;[positions[i], positions[j]] = [positions[j], positions[i]]
  }

  let removed = 0
  for (const pos of positions) {
    if (removed >= holes) break

    const { row, col } = pos
    const tempValue = solution[row][col]

    // 尝试移除
    board[row][col].value = 0

    // 检查是否还有唯一解
    const tempSolution: number[][] = Array(GRID_SIZE)
      .fill(null)
      .map(() => Array(GRID_SIZE).fill(0))

    if (countSolutions(board, tempSolution, 2) === 1) {
      removed++
    } else {
      // 恢复
      board[row][col].value = tempValue
    }
  }

  return removed
}

/**
 * 创建初始游戏状态
 */
export function createInitialState(difficulty: number = 0): SudokuState {
  const { GRID_SIZE } = SUDOKU_CONFIG
  const holes = SUDOKU_CONFIG.DIFFICULTIES[difficulty].holes

  // 生成完整解
  const solution = generateFullBoard()

  // 创建题目
  const board = createEmptyBoard()
  for (let r = 0; r < GRID_SIZE; r++) {
    for (let c = 0; c < GRID_SIZE; c++) {
      board[r][c].value = solution[r][c]
      board[r][c].isGiven = true
    }
  }

  // 挖洞
  const removed = removeNumbers(solution, board, holes)

  return {
    board,
    solution: copySolution(solution),
    cursorRow: 0,
    cursorCol: 0,
    difficulty,
    emptyCount: removed,
    isGameOver: false,
    selectedNumber: 1,
    hintCount: 3  // 初始 3 次提示
  }
}

/**
 * 放置数字
 */
export function placeNumber(state: SudokuState, num: number): void {
  const { cursorRow, cursorCol } = state
  const cell = state.board[cursorRow][cursorCol]

  // 固定格不能修改
  if (cell.isGiven) return

  // 放置数字
  cell.value = num

  // 更新冲突标记
  updateConflicts(state)

  // 检查是否完成
  checkWin(state)
}

/**
 * 擦除当前格子
 */
export function eraseCell(state: SudokuState): void {
  const { cursorRow, cursorCol } = state
  const cell = state.board[cursorRow][cursorCol]

  if (cell.isGiven) return

  cell.value = 0
  cell.isConflict = false

  // 重新计算空格子数
  recalcEmptyCount(state)
}

/**
 * 更新所有单元格的冲突标记
 */
export function updateConflicts(state: SudokuState): void {
  const { GRID_SIZE } = SUDOKU_CONFIG

  // 重置所有冲突
  for (let r = 0; r < GRID_SIZE; r++) {
    for (let c = 0; c < GRID_SIZE; c++) {
      state.board[r][c].isConflict = false
    }
  }

  // 检查每个已填格子
  for (let r = 0; r < GRID_SIZE; r++) {
    for (let c = 0; c < GRID_SIZE; c++) {
      const value = state.board[r][c].value
      if (value === 0) continue

      // 检查行
      for (let cc = 0; cc < GRID_SIZE; cc++) {
        if (cc !== c && state.board[r][cc].value === value) {
          state.board[r][c].isConflict = true
          state.board[r][cc].isConflict = true
        }
      }

      // 检查列
      for (let rr = 0; rr < GRID_SIZE; rr++) {
        if (rr !== r && state.board[rr][c].value === value) {
          state.board[r][c].isConflict = true
          state.board[rr][c].isConflict = true
        }
      }
    }
  }
}

/**
 * 重新计算空格子数
 */
function recalcEmptyCount(state: SudokuState): void {
  const { GRID_SIZE } = SUDOKU_CONFIG
  let count = 0
  for (let r = 0; r < GRID_SIZE; r++) {
    for (let c = 0; c < GRID_SIZE; c++) {
      if (state.board[r][c].value === 0) count++
    }
  }
  state.emptyCount = count
}

/**
 * 检查是否胜利
 */
export function checkWin(state: SudokuState): void {
  const { GRID_SIZE } = SUDOKU_CONFIG

  for (let r = 0; r < GRID_SIZE; r++) {
    for (let c = 0; c < GRID_SIZE; c++) {
      if (state.board[r][c].value === 0) return
    }
  }

  state.isGameOver = true
}

/**
 * 移动光标
 */
export function moveCursor(state: SudokuState, dx: number, dy: number): void {
  const { GRID_SIZE } = SUDOKU_CONFIG
  state.cursorCol = (state.cursorCol + dx + GRID_SIZE) % GRID_SIZE
  state.cursorRow = (state.cursorRow + dy + GRID_SIZE) % GRID_SIZE
}

/**
 * 设置光标位置
 */
export function setCursor(state: SudokuState, row: number, col: number): void {
  state.cursorRow = Math.max(0, Math.min(8, row))
  state.cursorCol = Math.max(0, Math.min(8, col))
}

/**
 * 选中数字
 */
export function selectNumber(state: SudokuState, num: number): void {
  state.selectedNumber = Math.max(1, Math.min(9, num))
}

/**
 * 提示（显示一个正确数字）
 */
export function hint(state: SudokuState): boolean {
  const { GRID_SIZE } = SUDOKU_CONFIG

  // 检查提示次数
  if (state.hintCount <= 0) {
    return false  // 提示次数已用完
  }

  // 找到第一个空格
  for (let r = 0; r < GRID_SIZE; r++) {
    for (let c = 0; c < GRID_SIZE; c++) {
      if (state.board[r][c].value === 0) {
        state.board[r][c].value = state.solution[r][c]
        state.board[r][c].isGiven = true
        state.emptyCount--
        state.hintCount--  // 消耗一次提示
        updateConflicts(state)
        checkWin(state)
        return true
      }
    }
  }
  return false
}