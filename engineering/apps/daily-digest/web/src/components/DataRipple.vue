<template>
  <div class="data-ripple" :class="{ 'is-empty': !hasData }">
    <!-- SVG 波纹可视化 -->
    <svg
      :width="svgWidth"
      :height="svgHeight"
      :viewBox="`0 0 ${svgWidth} ${svgHeight}`"
      xmlns="http://www.w3.org/2000/svg"
      @click="handleRippleClick"
    >
      <!-- 空数据占位 -->
      <line
        v-if="!hasData"
        class="ripple-empty"
        :x1="20"
        :y1="svgHeight / 2"
        :x2="svgWidth - 20"
        :y2="svgHeight / 2"
      />
      <!-- 波纹曲线 -->
      <path
        v-for="(wave, idx) in waves"
        :key="idx"
        class="ripple-wave"
        :d="wave.path"
        :style="{ opacity: wave.opacity }"
      />
      <!-- 填充区域 -->
      <path
        v-if="hasData"
        class="ripple-area"
        :d="areaPath"
      />
      <!-- 可点击热区（覆盖整个波纹区域） -->
      <rect
        class="ripple-hit"
        x="0"
        y="0"
        :width="svgWidth"
        :height="svgHeight"
      />
    </svg>

    <!-- 分类标签 -->
    <div class="ripple-labels">
      <span
        v-for="cat in categories"
        :key="cat.key"
        :class="{ active: activeCategory === cat.key }"
        @click="$emit('category-change', cat.key)"
      >
        {{ cat.label }}
        <sup v-if="cat.count !== undefined">{{ cat.count }}</sup>
      </span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

interface CategoryItem {
  key: string
  label: string
  count?: number
}

const props = withDefaults(defineProps<{
  /** 分类数据，用于生成波纹振幅和标签 */
  categories?: CategoryItem[]
  /** 当前选中的分类 */
  activeCategory?: string
  /** SVG 宽度 */
  svgWidth?: number
  /** SVG 高度 */
  svgHeight?: number
  /** 数据点（振幅归一化 0-1） */
  dataPoints?: number[]
}>(), {
  categories: () => [
    { key: 'db', label: '数据库' },
    { key: 'ai', label: 'AI' },
    { key: 'ml', label: 'ML' },
    { key: 'infra', label: '基础设施' },
    { key: 'sys', label: '系统' },
  ],
  activeCategory: '',
  svgWidth: 800,
  svgHeight: 80,
  dataPoints: () => [0.3, 0.6, 0.4, 0.8, 0.5, 0.7, 0.2, 0.9, 0.6, 0.3],
})

defineEmits<{
  /** 点击分类标签 */
  'category-change': [key: string]
}>()

const hasData = computed(() => props.dataPoints.length > 0)

/** 生成波纹曲线路径 */
const waves = computed(() => {
  if (!hasData.value) return []

  const w = props.svgWidth
  const h = props.svgHeight
  const pts = props.dataPoints
  const segmentW = w / (pts.length - 1)

  // 主波 + 2 层次波
  const offsets = [0, 0.4, -0.3]
  const scales = [1, 0.6, 0.35]

  return offsets.map((offset, layer) => {
    const amp = h * 0.35
    const baseY = h / 2 + offset * amp

    let d = `M 0 ${baseY}`
    for (let i = 0; i < pts.length; i++) {
      const x = i * segmentW
      const y = baseY - pts[i] * amp * scales[layer]
      // 贝塞尔平滑
      if (i === 0) {
        d += ` L ${x} ${y}`
      } else {
        const prevX = (i - 1) * segmentW
        const cx1 = prevX + segmentW * 0.25
        const cx2 = x - segmentW * 0.25
        const prevY = baseY - pts[i - 1] * amp * scales[layer]
        d += ` C ${cx1} ${prevY}, ${cx2} ${y}, ${x} ${y}`
      }
    }
    d += ` L ${w} ${h} L 0 ${h} Z`

    return {
      path: d,
      opacity: 1 - layer * 0.25,
    }
  })
})

/** 填充区域路径（最外层波向下闭合） */
const areaPath = computed(() => {
  if (!hasData.value || !waves.value.length) return ''
  return waves.value[0].path
})

/** 点击波纹区域 → 轮换分类 */
function handleRippleClick() {
  const cats = props.categories
  if (!cats.length) return
  const idx = cats.findIndex(c => c.key === props.activeCategory)
  const next = (idx + 1) % cats.length
  // 直接触发变更事件
  const event = new CustomEvent('ripple-cycle', { detail: cats[next].key })
  window.dispatchEvent(event)
}
</script>

<style scoped>
.data-ripple {
  position: relative;
  width: 100%;
  height: auto;
  margin-bottom: var(--space-6, 24px);
  background: var(--color-surface, #fff);
  border-radius: var(--radius-lg, 12px);
  box-shadow: var(--shadow-card, 0 1px 3px rgba(0,0,0,0.06));
  overflow: hidden;
  padding: var(--space-3, 12px) 0 0;
}

.data-ripple svg {
  display: block;
  width: 100%;
  height: auto;
}

.ripple-wave {
  fill: none;
  stroke-width: 2.5;
  stroke-linecap: round;
  stroke-linejoin: round;
  filter: drop-shadow(0 1px 2px rgba(45, 127, 249, 0.2));
  stroke: var(--color-brand, #2D7FF9);
}

.ripple-area {
  fill: color-mix(in srgb, var(--color-brand, #2D7FF9) 8%, transparent);
}

/* 空数据状态 — 平直线 */
.ripple-empty {
  fill: none;
  stroke: var(--color-text-tertiary, #9CA3AF);
  stroke-width: 1.5;
  stroke-dasharray: 4 4;
}

/* 分类标签 */
.ripple-labels {
  display: flex;
  justify-content: space-around;
  padding: 4px 20px 8px;
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-xs, 0.75rem);
  color: var(--color-text-tertiary, #9CA3AF);
}

.ripple-labels span {
  cursor: pointer;
  transition: color var(--transition-fast, 150ms ease);
}

.ripple-labels span:hover {
  color: var(--color-brand, #2D7FF9);
}

.ripple-labels span.active {
  color: var(--color-brand, #2D7FF9);
  font-weight: var(--weight-semibold, 600);
}

.ripple-labels sup {
  font-size: 0.65rem;
  margin-left: 2px;
  opacity: 0.7;
}

/* 点击热区 */
.ripple-hit {
  fill: transparent;
  stroke: transparent;
  cursor: pointer;
}

.ripple-hit:hover ~ .ripple-wave {
  stroke-width: 3;
}
</style>