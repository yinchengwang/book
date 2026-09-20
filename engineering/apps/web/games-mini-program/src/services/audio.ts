/**
 * @file services/audio.ts
 * @brief 音效 / 背景音乐管理
 */
import Taro from '@tarojs/taro'

type AudioContext = ReturnType<typeof Taro.createInnerAudioContext>

class AudioManagerClass {
  private cache = new Map<string, AudioContext>()
  private muted = false

  preload (sources: Record<string, string>) {
    for (const [name, src] of Object.entries(sources)) {
      const ctx = Taro.createInnerAudioContext()
      ctx.src = src
      this.cache.set(name, ctx)
    }
  }

  isReady (name: string) { return this.cache.has(name) }
  setMuted (m: boolean) { this.muted = m }
  isMuted () { return this.muted }

  playSfx (name: string) {
    if (this.muted) return
    this.cache.get(name)?.play()
  }

  playBgm (name: string) {
    if (this.muted) return
    const ctx = this.cache.get(name)
    if (ctx) { ctx.loop = true; ctx.play() }
  }

  stopBgm (name: string) { this.cache.get(name)?.stop() }
}

export const AudioManager = new AudioManagerClass()
