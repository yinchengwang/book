/**
 * @file hooks/useBestScore.ts
 * @brief 加载/保存单个游戏的最高分
 */
import { useCallback, useEffect, useState } from 'react'
import {
  getSnakeBestScore,
  updateSnakeBestScore,
  get2048BestScore,
  update2048BestScore
} from '@/utils/storage'

type GameKey = 'snake' | 'game2048'

export function useBestScore (game: GameKey) {
  const [best, setBest] = useState<number>(() => {
    return game === 'snake' ? getSnakeBestScore() : get2048BestScore()
  })

  const commit = useCallback((score: number) => {
    const ok = game === 'snake'
      ? updateSnakeBestScore(score)
      : update2048BestScore(score)
    if (ok) setBest(score)
    return ok
  }, [game])

  // 重新读取一次（覆盖 app.tsx 中 initStorage 的迁移）
  useEffect(() => {
    setBest(game === 'snake' ? getSnakeBestScore() : get2048BestScore())
  }, [game])

  return { best, commit }
}
