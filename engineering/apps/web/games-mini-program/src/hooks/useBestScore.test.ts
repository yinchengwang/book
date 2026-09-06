import { describe, it, expect, vi, beforeEach } from 'vitest'

vi.mock('@tarojs/taro', () => ({
  default: {
    getStorageSync: vi.fn(() => undefined),
    setStorageSync: vi.fn()
  }
}))

import { renderHook, act } from '@testing-library/react-hooks'
import { useBestScore } from './useBestScore'
import { updateSnakeBestScore } from '@/utils/storage'

describe('useBestScore', () => {
  beforeEach(() => {
    updateSnakeBestScore(0)
  })

  it('初始值等于当前最高分', () => {
    updateSnakeBestScore(120)
    const { result } = renderHook(() => useBestScore('snake'))
    expect(result.current.best).toBe(120)
  })

  it('提交更高分后更新', () => {
    const { result } = renderHook(() => useBestScore('snake'))
    act(() => result.current.commit(200))
    expect(result.current.best).toBe(200)
  })
})
