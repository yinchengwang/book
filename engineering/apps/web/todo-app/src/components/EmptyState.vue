<template>
  <div class="empty-state">
    <div class="empty-icon">{{ icon }}</div>
    <h3 class="empty-title">{{ title }}</h3>
    <p v-if="description" class="empty-desc">{{ description }}</p>
    <button v-if="actionText" class="btn btn-primary" @click="$emit('action')">
      {{ actionText }}
    </button>
    <slot />
  </div>
</template>

<script setup>
defineProps({
  icon: { type: String, default: '📋' },
  title: { type: String, default: '暂无数据' },
  description: { type: String, default: '' },
  actionText: { type: String, default: '' }
})
defineEmits(['action'])
</script>

<style scoped>
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 48px 24px;
  text-align: center;
  animation: fadeIn var(--transition-normal, 0.2s) ease-out;
}
.empty-icon { font-size: 48px; margin-bottom: 16px; opacity: 0.8; }
.empty-title { font-size: 18px; font-weight: 600; margin-bottom: 8px; color: var(--text); }
.empty-desc { font-size: 14px; color: var(--text-muted); margin-bottom: 20px; max-width: 300px; line-height: 1.6; }
</style>