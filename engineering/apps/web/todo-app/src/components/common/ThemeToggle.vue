<template>
  <div class="theme-toggle" role="radiogroup" aria-label="主题切换">
    <button
      v-for="opt in OPTIONS"
      :key="opt.value"
      :class="['theme-btn', { active: theme === opt.value }]"
      :aria-pressed="theme === opt.value"
      :data-test="'theme-' + opt.value"
      @click="setTheme(opt.value)"
    >
      <span aria-hidden="true">{{ opt.icon }}</span>
      <span class="sr-only">{{ opt.label }}</span>
    </button>
  </div>
</template>

<script setup lang="ts">
import { useTheme } from '@/composables/useTheme'
import type { Theme } from '@/types/ui'

const OPTIONS: Array<{ value: Theme; label: string; icon: string }> = [
  { value: 'light', label: '浅色', icon: '☀️' },
  { value: 'dark', label: '深色', icon: '🌙' },
  { value: 'auto', label: '跟随系统', icon: '🖥️' }
]

const { theme, setTheme } = useTheme()
</script>

<style scoped>
.theme-toggle {
  display: inline-flex;
  gap: 2px;
  padding: 2px;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: var(--bg-elev);
}
.theme-btn {
  padding: 4px 10px;
  border: none;
  background: transparent;
  cursor: pointer;
  border-radius: 4px;
  color: var(--text);
  font-size: 14px;
}
.theme-btn:hover { background: var(--bg-elev2); }
.theme-btn.active { background: var(--primary); color: white; }
.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  overflow: hidden;
  clip: rect(0 0 0 0);
}
</style>
