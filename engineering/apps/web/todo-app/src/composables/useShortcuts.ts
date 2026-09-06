import { onBeforeUnmount, ref } from 'vue'
import type { Ref } from 'vue'
import type { ShortcutBinding } from '@/types/ui'

type Handler = (event: KeyboardEvent) => void

const DEFAULT_BINDINGS: ShortcutBinding[] = [
  { key: 'n', modifiers: [], description: '新建任务', action: 'newTodo' },
  { key: '/', modifiers: [], description: '聚焦搜索', action: 'focusSearch' },
  { key: '?', modifiers: ['shift'], description: '显示帮助', action: 'showHelp' },
  { key: '1', modifiers: ['alt'], description: '列表视图', action: 'viewList' },
  { key: '2', modifiers: ['alt'], description: '看板视图', action: 'viewKanban' },
  { key: '3', modifiers: ['alt'], description: '统计视图', action: 'viewStats' },
  { key: '4', modifiers: ['alt'], description: '分组管理', action: 'viewGroups' },
  { key: 'Escape', modifiers: [], description: '取消', action: 'cancel' }
]

function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) return false
  const tag = target.tagName
  if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return true
  return target.isContentEditable
}

function matches(binding: ShortcutBinding, event: KeyboardEvent): boolean {
  if (event.key.toLowerCase() !== binding.key.toLowerCase()) return false
  for (const mod of binding.modifiers) {
    if (mod === 'ctrl' && !event.ctrlKey) return false
    if (mod === 'meta' && !event.metaKey) return false
    if (mod === 'alt' && !event.altKey) return false
    if (mod === 'shift' && !event.shiftKey) return false
  }
  return true
}

export interface ShortcutsApi {
  bindings: Ref<ShortcutBinding[]>
  addBinding: (binding: ShortcutBinding, handler: Handler) => void
  removeBinding: (action: string) => void
}

export function useShortcuts(): ShortcutsApi {
  const bindings = ref<ShortcutBinding[]>([...DEFAULT_BINDINGS])
  const handlers = new Map<string, Handler>()

  function listener(event: KeyboardEvent): void {
    if (isEditableTarget(event.target)) {
      if (event.key === 'Escape' && event.target instanceof HTMLElement) {
        event.target.blur()
      }
      return
    }
    for (const binding of bindings.value) {
      if (matches(binding, event)) {
        const handler = handlers.get(binding.action)
        if (handler) {
          event.preventDefault()
          handler(event)
          return
        }
      }
    }
  }

  document.addEventListener('keydown', listener)

  function addBinding(binding: ShortcutBinding, handler: Handler): void {
    const idx = bindings.value.findIndex((b) => b.action === binding.action)
    if (idx >= 0) bindings.value[idx] = binding
    else bindings.value.push(binding)
    handlers.set(binding.action, handler)
  }

  function removeBinding(action: string): void {
    bindings.value = bindings.value.filter((b) => b.action !== action)
    handlers.delete(action)
  }

  onBeforeUnmount(() => {
    document.removeEventListener('keydown', listener)
  })

  return { bindings, addBinding, removeBinding }
}
