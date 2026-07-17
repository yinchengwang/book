<template>
  <nav class="nav-bar">
    <div class="nav-inner">
      <router-link to="/" class="nav-logo">
        <span class="nav-logo-text">DailyDigest</span>
      </router-link>
      <div class="nav-links">
        <router-link to="/" class="nav-link" :class="{ active: $route.name === 'daily' }">
          每日
        </router-link>
        <router-link to="/search" class="nav-link" :class="{ active: $route.name === 'search' }">
          搜索
        </router-link>
        <router-link to="/collections" class="nav-link" :class="{ active: $route.name === 'collections' }">
          收藏
        </router-link>
        <router-link to="/settings" class="nav-link" :class="{ active: $route.name === 'settings' }">
          设置
        </router-link>
      </div>
    </div>
  </nav>
</template>

<style scoped>
.nav-bar {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  height: var(--nav-height, 56px);
  background: var(--color-nav-bg, rgba(255,255,255,0.85));
  backdrop-filter: blur(12px);
  border-bottom: 1px solid var(--color-nav-border, #E2E5EC);
  z-index: 100;
}

.nav-inner {
  max-width: var(--content-max-width, 960px);
  margin: 0 auto;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 var(--space-4, 16px);
}

.nav-logo-text {
  font-family: var(--font-display, 'Inter', sans-serif);
  font-weight: var(--weight-bold, 700);
  font-size: var(--text-lg, 1.0625rem);
  color: var(--color-brand, #2D7FF9);
}

.nav-links {
  display: flex;
  gap: var(--space-2, 8px);
}

.nav-link {
  padding: var(--space-1, 4px) var(--space-3, 12px);
  font-size: var(--text-sm, 0.8125rem);
  font-weight: var(--weight-medium, 500);
  color: var(--color-text-secondary, #5A6178);
  border-radius: var(--radius-md, 8px);
  transition: all var(--transition-fast, 150ms);
  text-decoration: none;
}

.nav-link:hover {
  color: var(--color-text-primary, #1A1A2E);
  background: var(--color-surface-hover, #F3F4F6);
}

.nav-link.active {
  color: var(--color-brand, #2D7FF9);
  background: color-mix(in srgb, var(--color-brand, #2D7FF9) 10%, transparent);
}
</style>