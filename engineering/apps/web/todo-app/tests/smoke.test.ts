import { describe, it, expect } from 'vitest'

describe('test environment', () => {
  it('runs vitest', () => {
    expect(1 + 1).toBe(2)
  })

  it('provides jsdom globals', () => {
    expect(typeof window).toBe('object')
    expect(typeof document).toBe('object')
  })
})
