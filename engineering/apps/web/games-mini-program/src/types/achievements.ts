/**
 * @file types/achievements.ts
 * @brief 成就系统的领域类型
 */

export type AchievementId =
  | 'snake_first_blood'
  | 'snake_score_100'
  | 'sudoku_first_clear'
  | 'sudoku_no_hint'
  | 'game2048_reach_2048'
  | 'game2048_reach_4096'
  | 'match3_first_clear'
  | 'match3_three_stars'

export interface Achievement {
  id: AchievementId
  unlockedAt: number  // ms epoch
}

export type AchievementProgress = Record<AchievementId, number>

export interface AchievementData {
  unlocked: Achievement[]
  progress: AchievementProgress
}
