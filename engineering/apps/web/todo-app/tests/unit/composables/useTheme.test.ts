import { describe, it, expect, beforeEach, afterEach } from 'vitest'
import { useTheme } from '@/composables/useTheme'

describe('useTheme', () => {
  beforeEach(() => {
    localStorage.clear()
    document.documentElement.removeAttribute('data-theme')
  })

  afterEach(() => {
    localStorage.clear()
    document.documentElement.removeAttribute('data-theme')
  })

  it('returns the current theme', () => {
    const { theme } = useTheme()
    expect(['light', 'dark', 'auto']).toContain(theme.value)
  })

  it('setTheme("dark") sets data-theme and persists', () => {
    const { theme, setTheme } = useTheme()
    setTheme('dark')
    expect(theme.value).toBe('dark')
    expect(document.documentElement.getAttribute('data-theme')).toBe('dark')
    expect(localStorage.getItem('theme')).toBe('dark')
  })

  it('setTheme("light") overrides dark', () => {
    const { setTheme } = useTheme()
    setTheme('dark')
    setTheme('light')
    expect(document.documentElement.getAttribute('data-theme')).toBe('light')
  })

  it('setTheme("auto") follows system preference', () => {
    const matchMediaMock = window.matchMedia as unknown as (q: string) => { matches: boolean }
    matchMediaMock.mockReturnValueOnce({ matches: true } as MediaQueryList)
    const { setTheme } = useTheme()
    setTheme('auto')
    expect(document.documentElement.getAttribute('data-theme')).toBe('dark')
  })

  it('reads stored theme on init', () => {
    localStorage.setItem('theme', 'dark')
    const { theme } = useTheme()
    expect(theme.value).toBe('dark')
  })
})
