/**
 * @file hooks/useVibration.ts
 * @brief 带节流的振动反馈
 */
import { useCallback, useRef } from 'react'
import Taro from '@tarojs/taro'

interface Options { throttleMs?: number; type?: 'light' | 'medium' | 'heavy' }

export function useVibration (options: Options = {}) {
  const { throttleMs = 80, type = 'light' } = options
  const lastRef = useRef(0)

  const trigger = useCallback(() => {
    const now = Date.now()
    if (now - lastRef.current < throttleMs) return
    lastRef.current = now
    try {
      Taro.vibrateShort({ type })
    } catch {
      // H5 / desktop fallback: silent
    }
  }, [throttleMs, type])

  return { trigger }
}
