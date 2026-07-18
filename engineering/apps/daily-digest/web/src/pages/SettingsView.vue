<template>
  <div class="settings-view">
    <h1 class="page-title">设置</h1>

    <div class="settings-section">
      <h3>主题切换</h3>
      <div class="theme-options">
        <button
          v-for="theme in themes"
          :key="theme.key"
          class="theme-btn"
          :class="{ active: currentTheme === theme.key }"
          @click="setTheme(theme.key)"
        >
          {{ theme.label }}
        </button>
      </div>
    </div>

    <div class="settings-section">
      <h3>关于</h3>
      <p class="about-text">
        DailyDigest — 每日数据库 & AI 技术速览<br />
        v0.1.0
      </p>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'

const themes = [
  { key: 'light', label: '浅色' },
  { key: 'dark', label: '暗色' },
  { key: 'eye', label: '护眼' },
  { key: 'warm', label: '暖色' },
]

const currentTheme = ref('light')

function setTheme(key: string) {
  currentTheme.value = key
  document.documentElement.setAttribute('data-theme', key)
  localStorage.setItem('daily-digest-theme', key)
}

onMounted(() => {
  const saved = localStorage.getItem('daily-digest-theme')
  if (saved && themes.some(t => t.key === saved)) {
    setTheme(saved)
  }
})
</script>

<style scoped>
.page-title {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-2xl, 1.5rem);
  font-weight: var(--weight-bold, 700);
  margin: 0 0 var(--space-6, 24px);
}

.settings-section {
  background: var(--color-surface, #fff);
  border-radius: var(--radius-lg, 12px);
  padding: var(--space-5, 20px);
  margin-bottom: var(--space-4, 16px);
  box-shadow: var(--shadow-card, 0 1px 3px rgba(0,0,0,0.06));
}

.settings-section h3 {
  font-size: var(--text-base, 0.9375rem);
  font-weight: var(--weight-semibold, 600);
  margin: 0 0 var(--space-4, 16px);
}

.theme-options {
  display: flex;
  gap: var(--space-2, 8px);
}

.theme-btn {
  flex: 1;
  padding: var(--space-3, 12px);
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-sm, 0.8125rem);
  font-weight: var(--weight-medium, 500);
  color: var(--color-text-secondary, #5A6178);
  background: var(--color-surface-hover, #F3F4F6);
  border: 2px solid transparent;
  border-radius: var(--radius-md, 8px);
  cursor: pointer;
  transition: all var(--transition-fast, 150ms);
}

.theme-btn:hover {
  color: var(--color-text-primary, #1A1A2E);
}

.theme-btn.active {
  color: var(--color-brand, #2D7FF9);
  border-color: var(--color-brand, #2D7FF9);
  background: color-mix(in srgb, var(--color-brand, #2D7FF9) 8%, transparent);
}

.about-text {
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-secondary, #5A6178);
  line-height: 1.6;
  margin: 0;
}
</style>