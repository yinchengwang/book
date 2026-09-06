import { describe, it, expect, vi } from 'vitest'

// vi.hoisted 把 mock 工厂提到 import 之上执行，避免 TDZ 错误
const { vibrateMock } = vi.hoisted(() => ({ vibrateMock: vi.fn() }))

vi.mock('@tarojs/taro', () => ({
  default: { vibrateShort: vibrateMock }
}))

import { renderHook, act } from '@testing-library/react'
import { useVibration } from './useVibration'

/**
 * @brief 节流逻辑：throttleMs 窗口内重复调用只触发一次；窗口外正常触发。
 */
describe('useVibration 节流逻辑', () => {
  it('首次 trigger 调用 vibrateShort', () => {
    const { result } = renderHook(() => useVibration({ throttleMs: 100 }))
    act(() => result.current.trigger())
    expect(vibrateMock).toHaveBeenCalledTimes(1)
  })

  it('throttleMs 窗口内重复 trigger 被丢弃', () => {
    vi.useFakeTimers()
    vibrateMock.mockClear()
    const { result } = renderHook(() => useVibration({ throttleMs: 100 }))
    act(() => {
      result.current.trigger()
      result.current.trigger()
      result.current.trigger()
    })
    expect(vibrateMock).toHaveBeenCalledTimes(1)
    vi.useRealTimers()
  })

  it('throttleMs 窗口外再次 trigger 正常触发', () => {
    vi.useFakeTimers()
    vibrateMock.mockClear()
    const { result } = renderHook(() => useVibration({ throttleMs: 100 }))
    act(() => { result.current.trigger() })
    // 推进时间到窗口外
    act(() => { vi.advanceTimersByTime(101) })
    act(() => { result.current.trigger() })
    expect(vibrateMock).toHaveBeenCalledTimes(2)
    vi.useRealTimers()
  })
})