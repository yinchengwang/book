<template>
  <div class="save-status-bar">
    <span v-if="status === 'saving'" class="save-saving">⏳ 保存中...</span>
    <span v-else-if="status === 'saved'" class="save-saved">✅ 已保存</span>
    <span v-else-if="status === 'error'" class="save-error">❌ 保存失败</span>
  </div>
</template>

<script setup>
defineProps({
  status: {
    type: String,
    default: 'idle'  // idle | saving | saved | error
  }
})
</script>

<style scoped>
.save-status-bar {
  display: inline-flex;
  align-items: center;
  padding: 2px 10px;
  border-radius: 12px;
  font-size: 12px;
  transition: all 0.3s;
}
.save-saving {
  color: #1976d2;
  background: #e3f2fd;
}
.save-saved {
  color: #388e3c;
  background: #e8f5e9;
}
.save-error {
  color: #d32f2f;
  background: #ffebee;
}
</style>