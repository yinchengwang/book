<template>
  <div class="skeleton-list">
    <div v-for="i in count" :key="i" class="skeleton-card">
      <div class="skeleton-line short"></div>
      <div class="skeleton-line"></div>
      <div class="skeleton-line medium"></div>
    </div>
  </div>
</template>

<script setup>
defineProps({ count: { type: Number, default: 3 } })
</script>

<style scoped>
.skeleton-card {
  background: var(--bg-elev);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 16px;
  margin-bottom: 8px;
}
.skeleton-line {
  height: 12px;
  background: linear-gradient(90deg, var(--bg-elev2) 25%, var(--border) 50%, var(--bg-elev2) 75%);
  background-size: 200% 100%;
  animation: shimmer 1.5s infinite;
  border-radius: 4px;
  margin-bottom: 8px;
}
.skeleton-line.short { width: 30%; }
.skeleton-line.medium { width: 60%; }
@keyframes shimmer {
  0% { background-position: 200% 0; }
  100% { background-position: -200% 0; }
}
</style>