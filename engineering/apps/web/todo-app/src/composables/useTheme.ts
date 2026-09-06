import { ref, watch } from 'vue'
import type { Ref } from 'vue'
import type { Theme } from '@/types/ui'

const STORAGE_KEY = 'theme'

function readStored(): Theme {
  const raw = localStorage.getItem(STORAGE_KEY)
  if (raw === 'light' || raw === 'dark' || raw === 'auto') return raw
  return 'auto'
}

function prefersDark(): boolean {
  return window.matchMedia?.('(prefers-color-scheme: dark)').matches ?? false
}

function applyTheme(theme: Theme): void {
  const effective = theme === 'auto' ? (prefersDark() ? 'dark' : 'light') : theme
  document.documentElement.setAttribute('data-theme', effective)
}

const theme: Ref<Theme> = ref(readStored())
applyTheme(theme.value)

let initialized = false

export function useTheme(): { theme: Ref<Theme>; setTheme: (next: Theme) => void } {
  // Re-read from storage on every call so external changes (and test isolation) are picked up.
  // The sync watcher below applies the theme whenever theme.value changes, so no explicit
  // applyTheme() call is needed here — calling it would double-consume mocked matchMedia in tests.
  theme.value = readStored()

  if (!initialized) {
    initialized = true
    // flush: 'sync' so setTheme() side effects apply immediately, matching synchronous callers
    watch(theme, (val) => {
      localStorage.setItem(STORAGE_KEY, val)
      applyTheme(val)
    }, { flush: 'sync' })

    window.matchMedia?.('(prefers-color-scheme: dark)').addEventListener?.('change', () => {
      if (theme.value === 'auto') applyTheme('auto')
    })
  }

  function setTheme(next: Theme): void {
    theme.value = next
  }

  return { theme, setTheme }
}
