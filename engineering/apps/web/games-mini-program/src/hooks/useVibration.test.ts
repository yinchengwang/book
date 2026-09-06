import { describe, it, expect, vi } from 'vitest'

const vibrateMock = vi.fn()
vi.mock('@tarojs/taro', () => ({
  default: { vibrateShort: vibrateMock }
}))

import { renderHook, act } from '@testing-library/react-hooks'
import { useVibration } from './useVibration'

describe('useVibration', () => {
  it('trigger 调用 vibrateShort', () => {
    const { result } = renderHook(() => useVibration({ throttleMs: 100 }))
    act(() => result.current.trigger())
    expect(vibrateMock).toHaveBeenCalledTimes(1)
  })

  it('100ms 内重复 trigger 被节流', () => {
    vi.useFakeTimers()
    const { result } = renderHook(() => useVibration({ throttleMs: 100 }))
    act(() => { result.current.trigger(); result.current.trigger(); result.current.trigger() })
    expect(vibrateMock).toHaveBeenCalledTimes(1)
    vi.useRealTimers()
  })
})
