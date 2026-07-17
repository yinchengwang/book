<template>
  <div class="filter-bar">
    <div class="search-input-wrapper">
      <span class="search-icon">🔍</span>
      <input v-model="searchQuery" class="form-input search-input" placeholder="搜索待办..." />
    </div>
    <select v-model="filter.status" class="form-select" @change="emit('filter', filter)">
      <option value="all">全部</option>
      <option value="open">未关闭</option>
      <option value="closed">已关闭</option>
      <option value="archived">已归档</option>
    </select>
    <select v-model="filter.priority" class="form-select" @change="emit('filter', filter)">
      <option :value="-1">全部优先级</option>
      <option v-for="(l, i) in PRIORITY_LABELS" :key="i" :value="i">{{ l }}</option>
    </select>
    <select v-model="filter.group_id" class="form-select" @change="emit('filter', filter)">
      <option :value="-1">全部分组</option>
      <option :value="0">未分组</option>
      <option v-for="g in groups" :key="g.id" :value="g.id">{{ g.name }}</option>
    </select>
    <button class="btn btn-primary btn-sm" @click="emit('new')">+ 新建</button>
  </div>
</template>

<script setup>
import { ref, watch, reactive, onUnmounted } from 'vue'

const props = defineProps({ groups: Array })
const emit = defineEmits(['filter', 'new'])

const PRIORITY_LABELS = ['紧急', '高', '中', '低', '无']
const filter = reactive({ search: '', status: 'all', priority: -1, group_id: -1 })
const searchQuery = ref('')

// 搜索防抖 300ms
let searchTimer = null
watch(searchQuery, (val) => {
  clearTimeout(searchTimer)
  searchTimer = setTimeout(() => {
    filter.search = val
    emit('filter', { ...filter })
  }, 300)
})

onUnmounted(() => clearTimeout(searchTimer))
</script>

<style scoped>
.filter-bar { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; padding: 12px 24px; background: var(--bg-elev); border-bottom: 1px solid var(--border); }
.filter-bar .form-select { width: auto; min-width: 120px; }
.search-input-wrapper { position: relative; flex: 1; max-width: 280px; }
.search-icon { position: absolute; left: 10px; top: 50%; transform: translateY(-50%); color: var(--text-muted); font-size: 14px; pointer-events: none; }
.search-input { padding-left: 32px !important; }
</style>
