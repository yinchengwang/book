/**
 * @file storage.ts
 * @brief 数据持久化存储抽象层
 *
 * 支持微信小程序和 H5 环境的统一存储接口
 */

// 存储键名前缀
const STORAGE_PREFIX = 'games_'

// 当前数据版本
const CURRENT_VERSION = 1

// 存储数据结构
export interface GameData {
  version: number
  snake: SnakeData
  sudoku: SudokuData
  game2048: Game2048Data
  match3: Match3Data
}

export interface SnakeData {
  bestScore: number
}

export interface SudokuData {
  bestScore: number
  chapters: ChapterProgress[]
}

export interface ChapterProgress {
  id: number
  unlocked: boolean
  stars: Record<number, number>  // levelId -> stars (1-3)
}

export interface Game2048Data {
  bestScore: number
}

export interface Match3Data {
  chapters: Record<number, Match3ChapterProgress>
}

export interface Match3ChapterProgress {
  unlocked: boolean
  stars: Record<number, number>  // levelId -> stars (1-3)
}

/**
 * 获取默认数据
 */
function getDefaultData(): GameData {
  return {
    version: CURRENT_VERSION,
    snake: { bestScore: 0 },
    sudoku: {
      bestScore: 0,
      chapters: [
        { id: 1, unlocked: true, stars: {} },
        { id: 2, unlocked: false, stars: {} },
        { id: 3, unlocked: false, stars: {} },
        { id: 4, unlocked: false, stars: {} },
        { id: 5, unlocked: false, stars: {} }
      ]
    },
    game2048: { bestScore: 0 },
    match3: {}
  }
}

/**
 * 获取存储键名
 */
function getKey(game: string): string {
  return `${STORAGE_PREFIX}${game}`
}

/**
 * 从存储读取数据
 */
export function getData<T>(game: string, defaultValue: T): T {
  try {
    const key = getKey(game)
    const data = Taro.getStorageSync(key)
    if (data) {
      return JSON.parse(data) as T
    }
  } catch (e) {
    console.warn(`读取 ${game} 数据失败:`, e)
  }
  return defaultValue
}

/**
 * 保存数据到存储
 */
export function setData<T>(game: string, data: T): boolean {
  try {
    const key = getKey(game)
    Taro.setStorageSync(key, JSON.stringify(data))
    return true
  } catch (e) {
    console.warn(`保存 ${game} 数据失败:`, e)
    return false
  }
}

// Taro 类型声明（兼容微信小程序和 H5）
declare const Taro: {
  getStorageSync: (key: string) => string | undefined
  setStorageSync: (key: string, value: string) => void
}

// ==================== 游戏数据访问接口 ====================

// 贪吃蛇
export function getSnakeBestScore(): number {
  return getData<SnakeData>('snake', { bestScore: 0 }).bestScore
}

export function setSnakeBestScore(score: number): boolean {
  return setData('snake', { bestScore: score })
}

export function updateSnakeBestScore(score: number): boolean {
  const current = getSnakeBestScore()
  if (score > current) {
    return setSnakeBestScore(score)
  }
  return true
}

// 2048
export function get2048BestScore(): number {
  return getData<Game2048Data>('game2048', { bestScore: 0 }).bestScore
}

export function set2048BestScore(score: number): boolean {
  return setData('game2048', { bestScore: score })
}

export function update2048BestScore(score: number): boolean {
  const current = get2048BestScore()
  if (score > current) {
    return set2048BestScore(score)
  }
  return true
}

// 数独
export function getSudokuData(): SudokuData {
  return getData<SudokuData>('sudoku', getDefaultData().sudoku)
}

export function setSudokuData(data: SudokuData): boolean {
  return setData('sudoku', data)
}

export function updateSudokuBestScore(score: number): boolean {
  const data = getSudokuData()
  if (score > data.bestScore) {
    data.bestScore = score
    return setSudokuData(data)
  }
  return true
}

export function unlockSudokuChapter(chapterId: number): boolean {
  const data = getSudokuData()
  const chapter = data.chapters.find(c => c.id === chapterId)
  if (chapter) {
    chapter.unlocked = true
    return setSudokuData(data)
  }
  return false
}

export function setSudokuChapterStars(chapterId: number, levelId: number, stars: number): boolean {
  const data = getSudokuData()
  const chapter = data.chapters.find(c => c.id === chapterId)
  if (chapter) {
    chapter.stars[levelId] = stars
    return setSudokuData(data)
  }
  return false
}

// 消消乐
export function getMatch3Data(): Match3Data {
  return getData<Match3Data>('match3', {})
}

export function setMatch3Data(data: Match3Data): boolean {
  return setData('match3', data)
}

export function unlockMatch3Chapter(chapterId: number): boolean {
  const data = getMatch3Data()
  if (!data.chapters) {
    data.chapters = {}
  }
  if (!data.chapters[chapterId]) {
    data.chapters[chapterId] = { unlocked: false, stars: {} }
  }
  data.chapters[chapterId].unlocked = true
  return setMatch3Data(data)
}

export function setMatch3ChapterStars(chapterId: number, levelId: number, stars: number): boolean {
  const data = getMatch3Data()
  if (!data.chapters) {
    data.chapters = {}
  }
  if (!data.chapters[chapterId]) {
    data.chapters[chapterId] = { unlocked: true, stars: {} }
  }
  data.chapters[chapterId].stars[levelId] = stars
  return setMatch3Data(data)
}

// ==================== 版本迁移 ====================

export interface StorageVersion {
  version: number
}

const VERSION_KEY = `${STORAGE_PREFIX}version`

export function migrateData(): boolean {
  try {
    // 读取版本号
    const storedVersion = Taro.getStorageSync(VERSION_KEY)
    const version = storedVersion ? JSON.parse(storedVersion) : 0

    if (version >= CURRENT_VERSION) {
      return true // 已是最新版本
    }

    // 执行迁移
    if (version < 1) {
      // v0 -> v1: 初始化各游戏数据结构
      const defaultData = getDefaultData()

      // 确保各游戏数据存在
      const snakeData = Taro.getStorageSync(getKey('snake'))
      if (!snakeData) {
        setData('snake', defaultData.snake)
      }

      const sudokuData = Taro.getStorageSync(getKey('sudoku'))
      if (!sudokuData) {
        setData('sudoku', defaultData.sudoku)
      }

      const game2048Data = Taro.getStorageSync(getKey('game2048'))
      if (!game2048Data) {
        setData('game2048', defaultData.game2048)
      }

      const match3Data = Taro.getStorageSync(getKey('match3'))
      if (!match3Data) {
        setData('match3', defaultData.match3)
      }
    }

    // 更新版本号
    Taro.setStorageSync(VERSION_KEY, JSON.stringify(CURRENT_VERSION))
    return true
  } catch (e) {
    console.error('数据迁移失败:', e)
    return false
  }
}

/**
 * 初始化存储（应用启动时调用）
 */
export function initStorage(): void {
  migrateData()
}