import { describe, it, expect, beforeEach, vi } from 'vitest'

const playMock = vi.fn()

vi.mock('@tarojs/taro', () => ({
  default: {
    createInnerAudioContext: vi.fn(() => ({
      src: '',
      play: playMock,
      stop: vi.fn(),
      pause: vi.fn(),
      onError: null,
      onEnded: null
    }))
  }
}))

import { AudioManager } from './audio'

describe('AudioManager', () => {
  beforeEach(() => { playMock.mockClear() })

  it('preload 创建音频上下文', () => {
    AudioManager.preload({ eat: 'https://example/eat.mp3' })
    expect(AudioManager.isReady('eat')).toBe(true)
  })

  it('playSfx 在非静音状态下调用 play', () => {
    AudioManager.preload({ eat: 'https://example/eat.mp3' })
    AudioManager.setMuted(false)
    AudioManager.playSfx('eat')
    expect(playMock).toHaveBeenCalled()
  })

  it('setMuted(true) 后不播放', () => {
    AudioManager.preload({ eat: 'https://example/eat.mp3' })
    AudioManager.setMuted(true)
    AudioManager.playSfx('eat')
    expect(playMock).not.toHaveBeenCalled()
  })
})
