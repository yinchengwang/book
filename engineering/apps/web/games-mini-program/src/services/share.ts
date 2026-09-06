/**
 * @file services/share.ts
 * @brief 各游戏分享卡片文案
 */

export interface ShareCard {
  title: string
  path: string
}

export function buildSnakeShare (score: number, best: number): ShareCard {
  return {
    title: `我在贪吃蛇里吃到了 ${score} 分，最高 ${best} 分，来挑战我！`,
    path: '/pages/snake/index'
  }
}

export function buildMatch3Share (chapter: number, level: number, score: number): ShareCard {
  return {
    title: `消消乐第 ${chapter}-${level} 关打到 ${score} 分，敢来挑战吗？`,
    path: '/pages/match3/index'
  }
}

export function build2048Share (score: number, best: number): ShareCard {
  return {
    title: `2048 合成到了 ${score} 分，最高 ${best} 分，来挑战我！`,
    path: '/pages/game2048/index'
  }
}

export function buildSudokuShare (chapter: number, level: number, stars: number): ShareCard {
  return {
    title: `数独第 ${chapter} 章第 ${level} 关拿到 ${stars} 星，你也来试试！`,
    path: '/pages/sudoku/index'
  }
}
