import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest'
import { useShortcuts } from '@/composables/useShortcuts'

describe('useShortcuts', () => {
  beforeEach(() => {
    document.body.innerHTML = ''
  })

  afterEach(() => {
    document.body.innerHTML = ''
    vi.restoreAllMocks()
  })

  it('starts with default bindings', () => {
    const { bindings } = useShortcuts()
    expect(bindings.value.length).toBeGreaterThan(0)
    expect(bindings.value.find((b) => b.action === 'newTodo')).toBeTruthy()
  })

  it('addBinding registers a new shortcut', () => {
    const handler = vi.fn()
    const { addBinding } = useShortcuts()
    addBinding({ key: 'p', modifiers: ['ctrl'], description: 'print', action: 'print' }, handler)
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'p', ctrlKey: true, bubbles: true }))
    expect(handler).toHaveBeenCalledTimes(1)
  })

  it('removeBinding unregisters a shortcut', () => {
    const handler = vi.fn()
    const { addBinding, removeBinding } = useShortcuts()
    addBinding({ key: 'p', modifiers: ['ctrl'], description: 'print', action: 'print' }, handler)
    removeBinding('print')
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'p', ctrlKey: true, bubbles: true }))
    expect(handler).not.toHaveBeenCalled()
  })

  it('does not fire for events from INPUT', () => {
    const handler = vi.fn()
    const { addBinding } = useShortcuts()
    addBinding({ key: 'p', modifiers: ['ctrl'], description: 'print', action: 'print' }, handler)
    const input = document.createElement('input')
    document.body.appendChild(input)
    input.dispatchEvent(new KeyboardEvent('keydown', { key: 'p', ctrlKey: true, bubbles: true }))
    expect(handler).not.toHaveBeenCalled()
  })

  it('matches shift modifier', () => {
    const handler = vi.fn()
    const { addBinding } = useShortcuts()
    addBinding({ key: '?', modifiers: ['shift'], description: 'help', action: 'help' }, handler)
    document.dispatchEvent(new KeyboardEvent('keydown', { key: '?', shiftKey: true, bubbles: true }))
    expect(handler).toHaveBeenCalledTimes(1)
  })

  it('requires all modifiers to match', () => {
    const handler = vi.fn()
    const { addBinding } = useShortcuts()
    addBinding({ key: 'p', modifiers: ['ctrl', 'shift'], description: 'print', action: 'print' }, handler)
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'p', ctrlKey: true, bubbles: true }))
    expect(handler).not.toHaveBeenCalled()
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'p', ctrlKey: true, shiftKey: true, bubbles: true }))
    expect(handler).toHaveBeenCalledTimes(1)
  })
})
