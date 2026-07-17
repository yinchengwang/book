<template>
  <div class="app-layout">
    <NavBar />
    <main class="main-content">
      <router-view />
    </main>
  </div>
</template>

<script setup lang="ts">
import NavBar from './components/NavBar.vue'
</script>

<style scoped>
.app-layout {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}

.main-content {
  flex: 1;
  max-width: var(--content-max-width, 960px);
  width: 100%;
  margin: 0 auto;
  padding: var(--space-4, 16px);
  padding-top: calc(var(--nav-height, 56px) + var(--space-6, 24px));
}
</style>
