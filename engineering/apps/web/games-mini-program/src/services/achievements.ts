/**
 * @file services/achievements.ts
 * @brief 成就规则引擎：每个游戏事件触发一组规则
 */
import { unlockAchievement, recordAchievementProgress } from '@/utils/storage'
import type { Achievement, AchievementId } from '@/types/achievements'

export type GameEvent =
  | { type: 'snake.eat'; score: number }
  | { type: 'snake.gameOver'; score: number }
  | { type: 'sudoku.complete'; hintCount: number }
  | { type: 'game2048.merge'; tileValue: number }
  | { type: 'match3.complete'; stars: number }

type Subscriber = (a: Achievement) => void
const subscribers = new Set<Subscriber>()

export function subscribe (fn: Subscriber): () => void {
  subscribers.add(fn)
  return () => subscribers.delete(fn)
}

function emit (a: Achievement | null) {
  if (!a) return
  // 每个订阅者独立 try/catch：一个抛出不影响其他监听器（toast、analytics、sound 等）
  subscribers.forEach(fn => {
    try {
      fn(a)
    } catch (err) {
      // 单个订阅者出错不影响其他订阅者；记录到 console 便于排查
      // eslint-disable-next-line no-console
      console.error('[achievements] subscriber threw:', err)
    }
  })
}

interface Rule {
  id: AchievementId
  test: (e: GameEvent) => boolean
  apply?: (e: GameEvent) => void
}

const RULES: Rule[] = [
  {
    id: 'snake_first_blood',
    test: e => e.type === 'snake.eat' && e.score >= 10
  },
  {
    id: 'snake_score_100',
    test: e => e.type === 'snake.gameOver' && e.score >= 100
  },
  {
    id: 'sudoku_first_clear',
    test: e => e.type === 'sudoku.complete'
  },
  {
    id: 'sudoku_no_hint',
    test: e => e.type === 'sudoku.complete' && e.hintCount === 0
  },
  {
    id: 'game2048_reach_2048',
    test: e => e.type === 'game2048.merge' && e.tileValue >= 2048
  },
  {
    id: 'game2048_reach_4096',
    test: e => e.type === 'game2048.merge' && e.tileValue >= 4096
  },
  {
    id: 'match3_first_clear',
    test: e => e.type === 'match3.complete'
  },
  {
    id: 'match3_three_stars',
    test: e => e.type === 'match3.complete' && e.stars === 3
  }
]

export function evaluate (event: GameEvent): Achievement[] {
  const unlocked: Achievement[] = []
  for (const rule of RULES) {
    if (rule.test(event)) {
      rule.apply?.(event)
      const a = unlockAchievement(rule.id)
      if (a) {
        unlocked.push(a)
        emit(a)
      }
    }
  }
  // 累计进度：贪吃蛇每吃一个 +10
  if (event.type === 'snake.eat') {
    recordAchievementProgress('snake_score_100', 10)
  }
  return unlocked
}

export function reset () {
  // 仅供测试
  subscribers.clear()
}