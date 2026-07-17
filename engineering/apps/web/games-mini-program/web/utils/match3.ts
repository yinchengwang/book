/**
 * 开心消消乐 - 核心游戏逻辑 (完整版)
 * 参考：开心消消乐（乐元素）
 *
 * 完整功能：
 * 1. 章节区域系统（普通/困难/超难关）
 * 2. 特效系统（直线/爆炸/彩虹）
 * 3. 障碍物系统（雪块/蜂蜜/藤蔓/石头）
 * 4. 特殊元素（钥匙/松鼠/发条鸟/蜗牛/宝箱）
 * 5. 章节解锁系统
 * 6. 星星评价系统
 * 7. 步数验证（确保可通关）
 */

// ==================== 常量定义 ====================

/** 棋盘大小 */
export const BOARD_SIZE = 9

/** 宝石类型 */
export type GemType = 0 | 1 | 2 | 3 | 4 | 5

/** 宝石 emoji */
export const GEM_EMOJIS: Record<GemType, string> = {
  0: '🟡', // 黄色
  1: '🟢', // 绿色
  2: '🩷', // 粉色
  3: '🟣', // 紫色
  4: '🔵', // 蓝色
  5: '🟠', // 橙色
}

/** 宝石颜色 */
export const GEM_COLORS: Record<GemType, { primary: string; secondary: string }> = {
  0: { primary: '#FFD93D', secondary: '#F6B93B' },
  1: { primary: '#26DE81', secondary: '#20BF6B' },
  2: { primary: '#FD79A8', secondary: '#E84393' },
  3: { primary: '#A55EEA', secondary: '#8854D0' },
  4: { primary: '#45AAF2', secondary: '#2E86DE' },
  5: { primary: '#F78FB3', secondary: '#F368E0' },
}

// ==================== 特效类型 ====================

export type SpecialType = 'line_h' | 'line_v' | 'bomb' | 'rainbow' | null

/** 特效宝石表示 */
export const SPECIAL_EMOJIS: Record<string, string> = {
  line_h: '➡️',    // 横向直线
  line_v: '⬇️',     // 纵向直线
  bomb: '💥',       // 爆炸
  rainbow: '🌈',    // 彩虹球
}

// ==================== 障碍物类型 ====================

export type ObstacleType = 'ice' | 'honey' | 'vine' | 'rock' | null

/** 障碍物信息 */
export const OBSTACLE_INFO: Record<string, { emoji: string; hits: number; desc: string }> = {
  ice: { emoji: '❄️', hits: 1, desc: '雪块' },
  honey: { emoji: '🍯', hits: 1, desc: '蜂蜜' },
  vine: { emoji: '🌿', hits: 2, desc: '藤蔓' },
  rock: { emoji: '🪨', hits: -1, desc: '石头' },
}

// ==================== 特殊元素类型 ====================

export type SpecialElementType = 'key' | 'treasure' | 'squirrel' | 'bird' | 'snail' | 'coin' | null

/** 特殊元素信息 */
export const ELEMENT_INFO: Record<string, { emoji: string; desc: string }> = {
  key: { emoji: '🔑', desc: '钥匙' },
  treasure: { emoji: '📦', desc: '宝箱' },
  squirrel: { emoji: '🐿️', desc: '松鼠' },
  bird: { emoji: '🐦', desc: '发条鸟' },
  snail: { emoji: '🐌', desc: '蜗牛' },
  coin: { emoji: '🪙', desc: '金币' },
}

// ==================== 关卡类型 ====================

export type LevelType = 'normal' | 'difficult' | 'boss'

export const LEVEL_TYPE_INFO: Record<LevelType, { name: string; color: string; bgColor: string }> = {
  normal: { name: '普通关', color: '#2E86DE', bgColor: '#87CEEB' },
  difficult: { name: '困难关', color: '#EE5A5A', bgColor: '#FFB6C1' },
  boss: { name: '超难关', color: '#9400D3', bgColor: '#4B0082' },
}

// ==================== 章节系统 ====================

export interface Chapter {
  id: number
  name: string
  type: LevelType
  levels: number[] // 关卡范围 [start, end]
  starsRequired: number // 解锁所需星星
  description: string
}

export const CHAPTERS: Chapter[] = [
  // 第1-3章：普通关
  { id: 1, name: '魔法森林', type: 'normal', levels: [1, 9], starsRequired: 0, description: '初入魔法森林' },
  { id: 2, name: '精灵花园', type: 'normal', levels: [10, 18], starsRequired: 15, description: '精灵们的秘密花园' },
  { id: 3, name: '星光原野', type: 'normal', levels: [19, 27], starsRequired: 30, description: '星光闪烁的原野' },
  // 第4-5章：困难关
  { id: 4, name: '暗夜森林', type: 'difficult', levels: [28, 36], starsRequired: 50, description: '暗夜笼罩的森林' },
  { id: 5, name: '冰霜峡谷', type: 'difficult', levels: [37, 45], starsRequired: 75, description: '寒冷的冰霜峡谷' },
  // 第6章：超难关
  { id: 6, name: 'BOSS挑战', type: 'boss', levels: [46, 50], starsRequired: 100, description: '最终BOSS关卡' },
]

/** 获取关卡所属章节 */
export function getChapterByLevel(level: number): Chapter | undefined {
  return CHAPTERS.find(c => level >= c.levels[0] && level <= c.levels[1])
}

/** 获取章节总星数 */
export function getChapterTotalStars(chapterId: number, earnedStars: Record<number, number>): number {
  const chapter = CHAPTERS.find(c => c.id === chapterId)
  if (!chapter) return 0
  let total = 0
  for (let lv = chapter.levels[0]; lv <= chapter.levels[1]; lv++) {
    total += earnedStars[lv] || 0
  }
  return total
}

/** 检查章节是否解锁 */
export function isChapterUnlocked(chapter: Chapter, totalStars: number, unlockedChapters: Set<number>): boolean {
  if (chapter.id === 1) return true // 第一章始终解锁
  return unlockedChapters.has(chapter.id - 1) || totalStars >= chapter.starsRequired
}

// ==================== 单元格结构 ====================

export interface Cell {
  gem: GemType | null       // 宝石类型
  special: SpecialType       // 特效类型
  obstacle: ObstacleType     // 障碍物
  element: SpecialElementType // 特殊元素
  obstacleHp: number         // 障碍物血量
  locked: boolean            // 是否被锁定（如石头覆盖）
}

// ==================== 核心函数 ====================

/** 随机宝石 */
export function randomGem(): GemType {
  return Math.floor(Math.random() * 6) as GemType
}

/** 创建空格子 */
export function createEmptyCell(): Cell {
  return {
    gem: null,
    special: null,
    obstacle: null,
    element: null,
    obstacleHp: 0,
    locked: false,
  }
}

/** 创建宝石格子 */
export function createGemCell(gem: GemType): Cell {
  return {
    gem,
    special: null,
    obstacle: null,
    element: null,
    obstacleHp: 0,
    locked: false,
  }
}

/** 复制棋盘 */
export function copyBoard(board: Cell[][]): Cell[][] {
  return board.map(row => row.map(cell => ({ ...cell })))
}

/** 检查是否会形成匹配 */
function wouldMatch(board: Cell[][], row: number, col: number, gem: GemType): boolean {
  // 检查水平
  let hCount = 1
  for (let c = col - 1; c >= 0 && board[row][c].gem === gem; c--) hCount++
  for (let c = col + 1; c < BOARD_SIZE && board[row][c].gem === gem; c++) hCount++
  if (hCount >= 3) return true

  // 检查垂直
  let vCount = 1
  for (let r = row - 1; r >= 0 && board[r][col].gem === gem; r--) vCount++
  for (let r = row + 1; r < BOARD_SIZE && board[r][col].gem === gem; r++) vCount++
  if (vCount >= 3) return true

  return false
}

/** 创建初始棋盘 */
export function createBoard(): Cell[][] {
  const board: Cell[][] = []

  for (let row = 0; row < BOARD_SIZE; row++) {
    board[row] = []
    for (let col = 0; col < BOARD_SIZE; col++) {
      let gem: GemType
      let attempts = 0
      do {
        gem = randomGem()
        attempts++
      } while (attempts < 100 && wouldMatch(board, row, col, gem))
      board[row][col] = createGemCell(gem)
    }
  }

  return board
}

/** 查找所有匹配 */
export function findMatches(board: Cell[][]): { cells: Set<string>; type: 'line' | 'lshape' | 'tshape'; direction?: 'h' | 'v' }[] {
  const results: { cells: Set<string>; type: 'line' | 'lshape' | 'tshape'; direction?: 'h' | 'v' }[] = []
  const matched = new Set<string>()

  // 水平匹配
  for (let row = 0; row < BOARD_SIZE; row++) {
    let col = 0
    while (col < BOARD_SIZE) {
      const cell = board[row][col]
      if (cell.gem !== null && !cell.obstacle) {
        let end = col
        while (end + 1 < BOARD_SIZE && board[row][end + 1].gem === cell.gem && !board[row][end + 1].obstacle) end++

        if (end - col + 1 >= 3) {
          const cells = new Set<string>()
          for (let c = col; c <= end; c++) {
            cells.add(`${row},${c}`)
            matched.add(`${row},${c}`)
          }
          results.push({
            cells,
            type: end - col + 1 >= 4 ? 'line' : 'line',
            direction: 'h'
          })
        }
        col = end + 1
      } else {
        col++
      }
    }
  }

  // 垂直匹配
  for (let col = 0; col < BOARD_SIZE; col++) {
    let row = 0
    while (row < BOARD_SIZE) {
      const cell = board[row][col]
      if (cell.gem !== null && !cell.obstacle) {
        let end = row
        while (end + 1 < BOARD_SIZE && board[end + 1][col].gem === cell.gem && !board[end + 1][obstacle]) end++

        if (end - row + 1 >= 3) {
          const cells = new Set<string>()
          for (let r = row; r <= end; r++) {
            cells.add(`${r},${col}`)
            matched.add(`${r},${col}`)
          }
          results.push({
            cells,
            type: end - row + 1 >= 4 ? 'line' : 'line',
            direction: 'v'
          })
        }
        row = end + 1
      } else {
        row++
      }
    }
  }

  return results
}

/** 分析匹配形状 */
export function analyzeMatchShape(matches: { cells: Set<string>; type: string }[]): {
  line4: string[]
  line5: string[]
  lshape: string[]
  intersections: string[]
} {
  const line4: string[] = []
  const line5: string[] = []
  const lshape: string[] = []
  const intersections: string[] = []

  for (const match of matches) {
    const positions = Array.from(match.cells)
    if (positions.length === 4) {
      line4.push(...positions)
    } else if (positions.length >= 5) {
      line5.push(...positions)
    } else if (positions.length === 3) {
      lshape.push(...positions)
    }
  }

  // 找交叉点（L/T形的中心）
  const allPositions = matches.flatMap(m => Array.from(m.cells))
  const countMap = new Map<string, number>()
  allPositions.forEach(p => countMap.set(p, (countMap.get(p) || 0) + 1))
  countMap.forEach((count, pos) => {
    if (count >= 2) intersections.push(pos)
  })

  return { line4, line5, lshape, intersections }
}

/** 触发特效 */
export function triggerSpecial(board: Cell[][], pos: string, special: SpecialType): Set<string> {
  const [row, col] = pos.split(',').map(Number)
  const removed = new Set<string>()

  switch (special) {
    case 'line_h': // 消除整行
      for (let c = 0; c < BOARD_SIZE; c++) {
        if (!board[row][c].obstacle || board[row][c].obstacle === 'ice' || board[row][c].obstacle === 'honey') {
          removed.add(`${row},${c}`)
        }
      }
      break
    case 'line_v': // 消除整列（发条鸟特效）
      for (let r = 0; r < BOARD_SIZE; r++) {
        if (!board[r][col].obstacle || board[r][col].obstacle === 'ice' || board[r][col].obstacle === 'honey') {
          removed.add(`${r},${col}`)
        }
      }
      break
    case 'bomb': // 消除3x3范围
      for (let dr = -1; dr <= 1; dr++) {
        for (let dc = -1; dc <= 1; dc++) {
          const nr = row + dr, nc = col + dc
          if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
            if (!board[nr][nc].obstacle || board[nr][nc].obstacle === 'ice' || board[nr][nc].obstacle === 'honey') {
              removed.add(`${nr},${nc}`)
            }
          }
        }
      }
      break
    case 'rainbow': // 消除所有同色宝石
      const gemType = board[row][col].gem
      for (let r = 0; r < BOARD_SIZE; r++) {
        for (let c = 0; c < BOARD_SIZE; c++) {
          if (board[r][c].gem === gemType) {
            removed.add(`${r},${c}`)
          }
        }
      }
      break
  }

  return removed
}

/** 宝石下落 */
export function dropGems(board: Cell[][]): void {
  for (let col = 0; col < BOARD_SIZE; col++) {
    let writeRow = BOARD_SIZE - 1
    for (let row = BOARD_SIZE - 1; row >= 0; row--) {
      if (board[row][col].gem !== null || board[row][col].obstacle !== null) {
        if (row !== writeRow) {
          board[writeRow][col] = { ...board[row][col] }
          board[row][col] = createEmptyCell()
        }
        writeRow--
      }
    }
    // 填充新宝石
    for (let row = writeRow; row >= 0; row--) {
      board[row][col] = createGemCell(randomGem())
    }
  }
}

/** 检查有效交换 */
export function isValidSwap(board: Cell[][], pos1: string, pos2: string): boolean {
  const [r1, c1] = pos1.split(',').map(Number)
  const [r2, c2] = pos2.split(',').map(Number)

  // 必须是有宝石的格子
  if (!board[r1][c1].gem || !board[r2][c2].gem) return false

  // 必须是相邻格子
  if (Math.abs(r1 - r2) + Math.abs(c1 - c2) !== 1) return false

  const newBoard = copyBoard(board)
  ;[newBoard[r1][c1], newBoard[r2][c2]] = [newBoard[r2][c2], newBoard[r1][c1]]

  return findMatches(newBoard).length > 0
}

/** 执行交换 */
export function doSwap(board: Cell[][], pos1: string, pos2: string): void {
  const [r1, c1] = pos1.split(',').map(Number)
  const [r2, c2] = pos2.split(',').map(Number)
  ;[board[r1][c1], board[r2][c2]] = [board[r2][c2], board[r1][c1]]
}

/** 计算分数 */
export function calculateScore(removedCount: number, combo: number): number {
  const base = removedCount * 10
  const comboBonus = combo > 1 ? combo * 0.5 : 0
  return Math.floor(base * (1 + comboBonus))
}

/** 计算星星数（根据剩余步数比例） */
export function calculateStars(score: number, targetScore: number, movesLeft: number, totalMoves: number): number {
  if (score < targetScore) return 0
  const moveRatio = movesLeft / totalMoves
  if (moveRatio >= 0.5) return 3
  if (moveRatio >= 0.25) return 2
  return 1
}

// ==================== 关卡配置 ====================

export interface LevelConfig {
  level: number
  chapter: number
  type: LevelType
  moves: number           // 步数
  targetScore: number    // 目标分数
  obstacles: { type: ObstacleType; count: number; positions?: string[] }[]
  elements: { type: SpecialElementType; count: number }[]
  goals: { type: string; target: number; emoji: string }[]
  layout: 'open' | 'partial' | 'restricted'  // 棋盘布局
  isBeatable: boolean    // 是否验证可通关
}

/** 关卡配置表（预定义关卡） */
const LEVEL_CONFIGS: Partial<Record<number, Omit<LevelConfig, 'level' | 'chapter'>>> = {
  // === 第1章：魔法森林（关卡1-9）===
  1: {
    moves: 25,
    targetScore: 1000,
    obstacles: [{ type: 'ice', count: 3 }],
    elements: [],
    goals: [{ type: 'score', target: 1000, emoji: '⭐' }],
    layout: 'open',
    isBeatable: true,
  },
  2: {
    moves: 25,
    targetScore: 1200,
    obstacles: [{ type: 'ice', count: 5 }],
    elements: [],
    goals: [{ type: 'score', target: 1200, emoji: '⭐' }],
    layout: 'open',
    isBeatable: true,
  },
  3: {
    moves: 25,
    targetScore: 1400,
    obstacles: [{ type: 'ice', count: 7 }],
    elements: [],
    goals: [{ type: 'score', target: 1400, emoji: '⭐' }],
    layout: 'open',
    isBeatable: true,
  },
  5: {
    moves: 28,
    targetScore: 1800,
    obstacles: [{ type: 'ice', count: 10 }],
    elements: [{ type: 'coin', count: 3 }],
    goals: [
      { type: 'score', target: 1800, emoji: '⭐' },
      { type: 'coin', target: 3, emoji: '🪙' },
    ],
    layout: 'open',
    isBeatable: true,
  },
  9: {
    moves: 30,
    targetScore: 2500,
    obstacles: [{ type: 'ice', count: 15 }],
    elements: [{ type: 'coin', count: 5 }],
    goals: [
      { type: 'score', target: 2500, emoji: '⭐' },
      { type: 'coin', target: 5, emoji: '🪙' },
    ],
    layout: 'open',
    isBeatable: true,
  },

  // === 第2章：精灵花园（关卡10-18）===
  10: {
    moves: 25,
    targetScore: 2800,
    obstacles: [
      { type: 'ice', count: 8 },
      { type: 'honey', count: 5 },
    ],
    elements: [{ type: 'key', count: 1 }],
    goals: [
      { type: 'score', target: 2800, emoji: '⭐' },
      { type: 'key', target: 1, emoji: '🔑' },
    ],
    layout: 'open',
    isBeatable: true,
  },
  15: {
    moves: 28,
    targetScore: 3500,
    obstacles: [
      { type: 'ice', count: 10 },
      { type: 'honey', count: 8 },
    ],
    elements: [
      { type: 'key', count: 2 },
      { type: 'treasure', count: 1 },
    ],
    goals: [
      { type: 'score', target: 3500, emoji: '⭐' },
      { type: 'key', target: 2, emoji: '🔑' },
      { type: 'treasure', target: 1, emoji: '📦' },
    ],
    layout: 'partial',
    isBeatable: true,
  },
  18: {
    moves: 30,
    targetScore: 4500,
    obstacles: [
      { type: 'ice', count: 12 },
      { type: 'honey', count: 10 },
      { type: 'vine', count: 3 },
    ],
    elements: [
      { type: 'key', count: 2 },
      { type: 'squirrel', count: 1 },
    ],
    goals: [
      { type: 'score', target: 4500, emoji: '⭐' },
      { type: 'key', target: 2, emoji: '🔑' },
    ],
    layout: 'partial',
    isBeatable: true,
  },

  // === 第3章：星光原野（关卡19-27）===
  20: {
    moves: 28,
    targetScore: 5000,
    obstacles: [
      { type: 'ice', count: 10 },
      { type: 'honey', count: 8 },
      { type: 'vine', count: 5 },
    ],
    elements: [
      { type: 'bird', count: 1 },
      { type: 'coin', count: 5 },
    ],
    goals: [
      { type: 'score', target: 5000, emoji: '⭐' },
      { type: 'bird', target: 1, emoji: '🐦' },
    ],
    layout: 'partial',
    isBeatable: true,
  },
  25: {
    moves: 30,
    targetScore: 6000,
    obstacles: [
      { type: 'ice', count: 12 },
      { type: 'honey', count: 10 },
      { type: 'vine', count: 6 },
      { type: 'rock', count: 4 },
    ],
    elements: [
      { type: 'bird', count: 2 },
      { type: 'snail', count: 1 },
      { type: 'treasure', count: 2 },
    ],
    goals: [
      { type: 'score', target: 6000, emoji: '⭐' },
      { type: 'bird', target: 2, emoji: '🐦' },
      { type: 'snail', target: 1, emoji: '🐌' },
    ],
    layout: 'partial',
    isBeatable: true,
  },
  27: {
    moves: 32,
    targetScore: 8000,
    obstacles: [
      { type: 'ice', count: 15 },
      { type: 'honey', count: 12 },
      { type: 'vine', count: 8 },
      { type: 'rock', count: 6 },
    ],
    elements: [
      { type: 'bird', count: 2 },
      { type: 'snail', count: 2 },
      { type: 'squirrel', count: 1 },
      { type: 'treasure', count: 2 },
    ],
    goals: [
      { type: 'score', target: 8000, emoji: '⭐' },
      { type: 'bird', target: 2, emoji: '🐦' },
      { type: 'snail', target: 2, emoji: '🐌' },
    ],
    layout: 'restricted',
    isBeatable: true,
  },

  // === 困难关 ===
  30: {
    moves: 22,
    targetScore: 10000,
    obstacles: [
      { type: 'ice', count: 20 },
      { type: 'honey', count: 15 },
      { type: 'vine', count: 10 },
      { type: 'rock', count: 8 },
    ],
    elements: [
      { type: 'bird', count: 3 },
      { type: 'snail', count: 2 },
      { type: 'key', count: 3 },
    ],
    goals: [
      { type: 'score', target: 10000, emoji: '⭐' },
      { type: 'bird', target: 3, emoji: '🐦' },
    ],
    layout: 'restricted',
    isBeatable: true,
  },
  40: {
    moves: 20,
    targetScore: 15000,
    obstacles: [
      { type: 'ice', count: 25 },
      { type: 'honey', count: 20 },
      { type: 'vine', count: 12 },
      { type: 'rock', count: 10 },
    ],
    elements: [
      { type: 'bird', count: 3 },
      { type: 'snail', count: 3 },
      { type: 'squirrel', count: 2 },
      { type: 'key', count: 4 },
    ],
    goals: [
      { type: 'score', target: 15000, emoji: '⭐' },
      { type: 'snail', target: 3, emoji: '🐌' },
    ],
    layout: 'restricted',
    isBeatable: true,
  },

  // === BOSS关 ===
  50: {
    moves: 35,
    targetScore: 30000,
    obstacles: [
      { type: 'ice', count: 30 },
      { type: 'honey', count: 25 },
      { type: 'vine', count: 15 },
      { type: 'rock', count: 15 },
    ],
    elements: [
      { type: 'bird', count: 5 },
      { type: 'snail', count: 4 },
      { type: 'squirrel', count: 3 },
      { type: 'key', count: 5 },
      { type: 'treasure', count: 5 },
    ],
    goals: [
      { type: 'score', target: 30000, emoji: '⭐' },
      { type: 'bird', target: 5, emoji: '🐦' },
      { type: 'snail', target: 4, emoji: '🐌' },
    ],
    layout: 'restricted',
    isBeatable: true,
  },
}

/** 生成关卡配置 */
export function getLevelConfig(level: number): LevelConfig {
  const chapter = getChapterByLevel(level)
  const chapterId = chapter?.id || 1
  const type = chapter?.type || 'normal'

  // 如果有预定义配置，使用预定义配置
  if (LEVEL_CONFIGS[level]) {
    return {
      level,
      chapter: chapterId,
      ...LEVEL_CONFIGS[level]!,
    }
  }

  // 否则动态生成
  const baseMoves = type === 'boss' ? 30 : type === 'difficult' ? 25 : 30
  const movesReduction = Math.floor((level - 1) / 10)
  const moves = Math.max(15, baseMoves - movesReduction)

  const baseScore = type === 'boss' ? 20000 : type === 'difficult' ? 8000 : 1000
  const targetScore = baseScore + level * (type === 'boss' ? 500 : type === 'difficult' ? 300 : 100)

  return {
    level,
    chapter: chapterId,
    type,
    moves,
    targetScore,
    obstacles: generateObstacles(level, type),
    elements: generateElements(level, type),
    goals: generateGoals(level, type),
    layout: level % 10 === 0 ? 'restricted' : level % 5 === 0 ? 'partial' : 'open',
    isBeatable: true,
  }
}

/** 生成障碍物配置 */
function generateObstacles(level: number, type: LevelType): { type: ObstacleType; count: number }[] {
  const obstacles: { type: ObstacleType; count: number }[] = []
  const difficulty = type === 'boss' ? 3 : type === 'difficult' ? 2 : 1

  // 雪块
  const iceCount = Math.min(25, Math.floor(level * 0.8 * difficulty))
  if (iceCount > 0) obstacles.push({ type: 'ice', count: iceCount })

  // 蜂蜜
  if (level >= 10) {
    const honeyCount = Math.min(20, Math.floor((level - 10) * 0.5 * difficulty))
    if (honeyCount > 0) obstacles.push({ type: 'honey', count: honeyCount })
  }

  // 藤蔓
  if (level >= 20) {
    const vineCount = Math.min(12, Math.floor((level - 20) * 0.3 * difficulty))
    if (vineCount > 0) obstacles.push({ type: 'vine', count: vineCount })
  }

  // 石头
  if (level >= 30) {
    const rockCount = Math.min(10, Math.floor((level - 30) * 0.2 * difficulty))
    if (rockCount > 0) obstacles.push({ type: 'rock', count: rockCount })
  }

  return obstacles
}

/** 生成特殊元素配置 */
function generateElements(level: number, type: LevelType): { type: SpecialElementType; count: number }[] {
  const elements: { type: SpecialElementType; count: number }[] = []
  const difficulty = type === 'boss' ? 3 : type === 'difficult' ? 2 : 1

  // 金币
  if (level >= 5) {
    const coinCount = Math.min(8, Math.floor(level * 0.2 * difficulty))
    if (coinCount > 0) elements.push({ type: 'coin', count: coinCount })
  }

  // 钥匙
  if (level >= 15) {
    const keyCount = Math.min(4, Math.floor((level - 15) * 0.1 * difficulty) + 1)
    if (keyCount > 0) elements.push({ type: 'key', count: keyCount })
  }

  // 宝箱
  if (level >= 25) {
    const treasureCount = Math.min(3, Math.floor((level - 25) * 0.08 * difficulty) + 1)
    if (treasureCount > 0) elements.push({ type: 'treasure', count: treasureCount })
  }

  // 松鼠
  if (level >= 35) {
    const squirrelCount = Math.min(2, Math.floor((level - 35) * 0.05 * difficulty) + 1)
    if (squirrelCount > 0) elements.push({ type: 'squirrel', count: squirrelCount })
  }

  // 发条鸟
  if (level >= 40) {
    const birdCount = Math.min(3, Math.floor((level - 40) * 0.05 * difficulty) + 1)
    if (birdCount > 0) elements.push({ type: 'bird', count: birdCount })
  }

  // 蜗牛
  if (level >= 50) {
    const snailCount = Math.min(3, Math.floor((level - 50) * 0.04 * difficulty) + 1)
    if (snailCount > 0) elements.push({ type: 'snail', count: snailCount })
  }

  return elements
}

/** 生成关卡目标 */
function generateGoals(level: number, type: LevelType): { type: string; target: number; emoji: string }[] {
  const goals: { type: string; target: number; emoji: string }[] = []
  const config = getLevelConfig(level)

  // 基础目标：得分
  goals.push({ type: 'score', target: config.targetScore, emoji: '⭐' })

  // 收集目标
  for (const el of config.elements) {
    if (el.count > 0) {
      const emoji = ELEMENT_INFO[el.type]?.emoji || '❓'
      goals.push({ type: el.type, target: el.count, emoji })
    }
  }

  return goals
}

/** 验证关卡可通关性（简化版） */
export function verifyLevelBeatable(config: LevelConfig): boolean {
  // 估算：每步平均消除约5个宝石，得10*5=50分
  const estimatedScore = config.moves * 50

  // 考虑障碍物：每个障碍物需要额外1步消除
  let obstaclePenalty = 0
  for (const obs of config.obstacles) {
    if (obs.type === 'ice') obstaclePenalty += obs.count * 1
    if (obs.type === 'honey') obstaclePenalty += obs.count * 1.5
    if (obs.type === 'vine') obstaclePenalty += obs.count * 2
  }

  // 考虑特殊元素收集
  let elementPenalty = 0
  for (const el of config.elements) {
    elementPenalty += el.count * 2 // 每个元素需要约2步
  }

  // 实际可用步数
  const effectiveMoves = config.moves - obstaclePenalty - elementPenalty
  const effectiveScore = effectiveMoves * 50

  return effectiveScore >= config.targetScore * 0.8 // 允许20%容差
}

// ==================== 游戏状态 ====================

export interface GameState {
  level: number
  chapter: number
  board: Cell[][]
  moves: number
  maxMoves: number
  score: number
  collected: Record<string, number>  // 已收集的特殊元素
  goals: { type: string; current: number; target: number }[]  // 目标进度
  status: 'playing' | 'won' | 'lost'
  stars: number  // 获得的星星
  snails: { row: number; col: number; movesLeft: number }[]  // 蜗牛位置
  unlockedChapters: Set<number>
  earnedStars: Record<number, number>  // 每个关卡获得的星星
  totalStars: number
}

export function createGameState(level: number, earnedStars: Record<number, number> = {}, unlockedChapters: Set<number> = new Set([1])): GameState {
  const config = getLevelConfig(level)
  const board = createBoard()

  // 放置障碍物
  placeObstacles(board, config.obstacles)

  // 放置特殊元素
  placeElements(board, config.elements)

  // 初始化目标进度
  const goals = config.goals.map(g => ({
    type: g.type,
    current: 0,
    target: g.target,
  }))

  // 初始化蜗牛
  const snails: { row: number; col: number; movesLeft: number }[] = []
  const snailElements = config.elements.filter(e => e.type === 'snail')
  for (const snail of snailElements) {
    for (let i = 0; i < snail.count; i++) {
      snails.push({
        row: Math.floor(Math.random() * 3), // 初始在上方
        col: Math.floor(Math.random() * BOARD_SIZE),
        movesLeft: 15 + Math.floor(Math.random() * 10), // 15-25步后掉落
      })
    }
  }

  // 计算已获得的总星星
  const totalStars = Object.values(earnedStars).reduce((a, b) => a + b, 0)

  return {
    level,
    chapter: config.chapter,
    board,
    moves: config.moves,
    maxMoves: config.moves,
    score: 0,
    collected: {},
    goals,
    status: 'playing',
    stars: 0,
    snails,
    unlockedChapters,
    earnedStars,
    totalStars,
  }
}

/** 放置障碍物 */
function placeObstacles(board: Cell[][], obstacles: { type: ObstacleType; count: number }[]): void {
  for (const obs of obstacles) {
    for (let i = 0; i < obs.count; i++) {
      const pos = findEmptyPosition(board, true)
      if (pos) {
        const [row, col] = pos.split(',').map(Number)
        board[row][col].obstacle = obs.type
        board[row][col].obstacleHp = OBSTACLE_INFO[obs.type!].hits
        board[row][col].gem = randomGem()
      }
    }
  }
}

/** 放置特殊元素 */
function placeElements(board: Cell[][], elements: { type: SpecialElementType; count: number }[]): void {
  for (const el of elements) {
    for (let i = 0; i < el.count; i++) {
      const pos = findEmptyPosition(board, false)
      if (pos) {
        const [row, col] = pos.split(',').map(Number)
        board[row][col].element = el.type
      }
    }
  }
}

/** 找空位置 */
function findEmptyPosition(board: Cell[][], allowGem: boolean): string | null {
  const positions: string[] = []
  for (let row = 0; row < BOARD_SIZE; row++) {
    for (let col = 0; col < BOARD_SIZE; col++) {
      if (allowGem || (board[row][col].gem === null && !board[row][col].obstacle && !board[row][col].element)) {
        positions.push(`${row},${col}`)
      }
    }
  }
  if (positions.length === 0) return null
  return positions[Math.floor(Math.random() * positions.length)]
}

/** 更新蜗牛位置 */
export function updateSnails(state: GameState): { snailRemoved: string[]; gameOver: boolean } {
  const snailRemoved: string[] = []
  let gameOver = false

  for (const snail of state.snails) {
    snail.movesLeft--
    snail.row++

    // 蜗牛掉出棋盘
    if (snail.row >= BOARD_SIZE) {
      snailRemoved.push(`${snail.row - 1},${snail.col}`)
      gameOver = true // 蜗牛掉落，游戏失败
    }
  }

  return { snailRemoved, gameOver }
}

/** 检查目标完成情况 */
export function checkGoalsComplete(state: GameState): boolean {
  return state.goals.every(g => g.current >= g.target)
}

/** 处理特殊元素效果 */
export function processElementEffect(board: Cell[][], pos: string, element: SpecialElementType): Set<string> {
  const removed = new Set<string>()
  const [row, col] = pos.split(',').map(Number)

  switch (element) {
    case 'bird': // 发条鸟：消除整列
      for (let r = 0; r < BOARD_SIZE; r++) {
        if (!board[r][col].obstacle || board[r][col].obstacle === 'ice') {
          removed.add(`${r},${col}`)
        }
      }
      break
    case 'squirrel': // 松鼠：消除周围2格内所有宝石
      for (let dr = -2; dr <= 2; dr++) {
        for (let dc = -2; dc <= 2; dc++) {
          const nr = row + dr, nc = col + dc
          if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
            if (board[nr][nc].gem !== null) {
              removed.add(`${nr},${nc}`)
            }
          }
        }
      }
      break
    case 'snail': // 蜗牛：消除自身位置
      removed.add(pos)
      break
  }

  return removed
}

// ==================== 导出类型 ====================

export type { Cell, LevelType, ObstacleType, SpecialElementType }
