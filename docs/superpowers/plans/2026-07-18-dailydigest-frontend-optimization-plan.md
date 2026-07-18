# DailyDigest 前端优化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现分页组件重构（页码标签 + 每页条数选择器）、翻译功能（后端 LLM + 前端按钮）、UI 美化（时间线卡片列表）

**Architecture:** 后端新增 `/api/v1/translate` 路由复用现有 Summarizer 进行 LLM 翻译；前端重构 Pagination.vue 为带页码标签的完整分页器，ItemCard.vue 增加翻译交互和视觉美化

**Tech Stack:** Vue 3 + TypeScript + Vite（前端），FastAPI + SQLAlchemy（后端），CSS 自定义属性（四色主题）

## Global Constraints

- 所有颜色使用 CSS 变量（`var(--color-*)`），四色主题自动适配
- 代码注释使用中文
- 翻译接口需 LLM_API_KEY 配置，无 key 时返回原文
- 分页默认每页 10 条
- 每页条数选项：10 / 20 / 50 / 100

---

### Task 1: 后端翻译接口

**Files:**
- Create: `engineering/apps/daily-digest/backend/app/routers/translate.py`
- Modify: `engineering/apps/daily-digest/backend/app/routers/__init__.py`（第 1-9 行和 11-18 行）
- Modify: `engineering/apps/daily-digest/backend/app/main.py`（第 4-8 行和第 31-38 行）

**Interfaces:**
- Produces: `POST /api/v1/translate` → `{"translated_text": "..."}`

- [ ] **Step 1: 创建 translate.py 路由**

```python
import logging
from pydantic import BaseModel
from fastapi import APIRouter
from app.services.processor.summarizer import Summarizer

logger = logging.getLogger("router.translate")
router = APIRouter(tags=["translate"])

class TranslateRequest(BaseModel):
    text: str
    source_lang: str = "en"
    target_lang: str = "zh"

class TranslateResponse(BaseModel):
    translated_text: str

@router.post("/api/v1/translate", response_model=TranslateResponse)
async def translate(req: TranslateRequest):
    """调用 LLM 翻译英文技术内容为中文"""
    summarizer = Summarizer()
    prompt = f"请将以下英文技术内容翻译成中文，保持专业术语准确，不要添加解释：\n\n{req.text}"
    translated = await summarizer._call_claude(prompt) if "claude" in summarizer.model.lower() else await summarizer._call_openai(prompt)
    if not translated:
        translated = req.text  # fallback 返回原文
    return TranslateResponse(translated_text=translated)
```

- [ ] **Step 2: 在 __init__.py 注册 translate_router**

在 `engineering/apps/daily-digest/backend/app/routers/__init__.py` 中添加：
```python
from app.routers.translate import router as translate_router
```

并在 `__all__` 列表中添加 `"translate_router"`。

- [ ] **Step 3: 在 main.py 注册路由**

在 `engineering/apps/daily-digest/backend/app/main.py` 中：
1. 在 import 中添加 `translate_router,`
2. 在 `app.include_router(stats_router)` 之后添加：
```python
app.include_router(translate_router)
```

- [ ] **Step 4: 启动后端验证翻译接口**

```bash
cd c:/code/book/engineering/apps/daily-digest/backend
pip install httpx pydantic_settings 2>&1 | tail -3
uvicorn app.main:app --reload &
sleep 3
curl -s -X POST http://localhost:8000/api/v1/translate \
  -H "Content-Type: application/json" \
  -d '{"text": "Llama 3 is an efficient open-source large language model released by Meta AI."}' | python -m json.tool
```

没有 LLM_API_KEY 时预期返回 `{"translated_text": "原文"}`（fallback）。有 key 时返回翻译后中文。

- [ ] **Step 5: 提交**

```bash
cd c:/code/book
git add engineering/apps/daily-digest/backend/app/routers/translate.py \
       engineering/apps/daily-digest/backend/app/routers/__init__.py \
       engineering/apps/daily-digest/backend/app/main.py
git commit -m "feat: 新增翻译接口 POST /api/v1/translate（LLM 翻译）"
git push origin main
```

---

### Task 2: 重构分页组件（Pagination.vue）

**Files:**
- Rewrite: `engineering/apps/daily-digest/web/src/components/Pagination.vue`

**Interfaces:**
- Consumes: `props: { modelValue: number, total: number, size: number }`
- Emits: `update:modelValue`, `update:size`
- Adds new prop `sizeOptions?: number[]` (default `[10, 20, 50, 100]`)

- [ ] **Step 1: 重写 Pagination.vue**

```vue
<template>
  <div class="pagination" v-if="totalPages > 1">
    <!-- 每页条数选择器 -->
    <div class="page-size-selector">
      <select
        class="page-size-select"
        :value="size"
        @change="$emit('update:size', Number(($event.target as HTMLSelectElement).value))"
      >
        <option v-for="opt in sizeOptions" :key="opt" :value="opt">
          {{ opt }}条/页
        </option>
      </select>
    </div>

    <!-- 上一页 -->
    <button
      class="page-btn"
      :disabled="modelValue <= 1"
      @click="go(modelValue - 1)"
    >
      ← 上一页
    </button>

    <!-- 页码标签 -->
    <template v-for="page in visiblePages" :key="page">
      <span v-if="page === '...'" class="page-ellipsis">...</span>
      <button
        v-else
        class="page-btn page-num"
        :class="{ active: page === modelValue }"
        @click="go(page)"
      >
        {{ page }}
      </button>
    </template>

    <!-- 下一页 -->
    <button
      class="page-btn"
      :disabled="modelValue >= totalPages"
      @click="go(modelValue + 1)"
    >
      下一页 →
    </button>

    <!-- 页码信息 -->
    <span class="page-info">{{ modelValue }}/{{ totalPages }}</span>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = withDefaults(defineProps<{
  modelValue: number
  total: number
  size: number
  sizeOptions?: number[]
}>(), {
  sizeOptions: () => [10, 20, 50, 100],
})

const emit = defineEmits<{
  'update:modelValue': [page: number]
  'update:size': [size: number]
}>()

const totalPages = computed(() => Math.ceil(props.total / props.size) || 1)

/** 计算可见页码（当前页前后 2 页，首尾页始终可见） */
const visiblePages = computed(() => {
  const tp = totalPages.value
  const cur = props.modelValue
  if (tp <= 7) {
    // 小于等于 7 页全部显示
    return Array.from({ length: tp }, (_, i) => i + 1)
  }
  const pages: (number | string)[] = []
  // 始终显示第 1 页
  pages.push(1)
  // 计算可见范围
  const start = Math.max(2, cur - 2)
  const end = Math.min(tp - 1, cur + 2)
  if (start > 2) pages.push('...')
  for (let i = start; i <= end; i++) pages.push(i)
  if (end < tp - 1) pages.push('...')
  // 始终显示最后一页
  pages.push(tp)
  return pages
})

function go(page: number) {
  if (page >= 1 && page <= totalPages.value) {
    emit('update:modelValue', page)
  }
}
</script>

<style scoped>
.pagination {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: var(--space-2, 8px);
  padding: var(--space-6, 24px) 0;
  flex-wrap: wrap;
}

.page-size-selector {
  margin-right: var(--space-3, 12px);
}

.page-size-select {
  padding: var(--space-1, 4px) var(--space-3, 12px);
  font-family: var(--font-body, sans-serif);
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-primary, #1A1A2E);
  background: var(--color-surface, #fff);
  border: 1px solid var(--color-border, #E2E5EC);
  border-radius: var(--radius-md, 8px);
  cursor: pointer;
  outline: none;
}

.page-size-select:focus {
  border-color: var(--color-brand, #2D7FF9);
}

.page-btn {
  min-width: 36px;
  height: 36px;
  padding: 0 var(--space-2, 8px);
  font-family: var(--font-display, 'Inter', sans-serif);
  font-size: var(--text-sm, 0.8125rem);
  font-weight: var(--weight-medium, 500);
  color: var(--color-text-primary, #1A1A2E);
  background: var(--color-surface, #fff);
  border: 1px solid var(--color-border, #E2E5EC);
  border-radius: var(--radius-md, 8px);
  cursor: pointer;
  transition: all var(--transition-fast, 150ms);
  display: inline-flex;
  align-items: center;
  justify-content: center;
}

.page-btn:hover:not(:disabled):not(.active) {
  border-color: var(--color-brand, #2D7FF9);
  color: var(--color-brand, #2D7FF9);
}

.page-btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.page-num.active {
  color: #fff;
  background: var(--color-brand, #2D7FF9);
  border-color: var(--color-brand, #2D7FF9);
  font-weight: var(--weight-semibold, 600);
}

.page-ellipsis {
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-sm, 0.8125rem);
  color: var(--color-text-tertiary, #9CA3AF);
  width: 24px;
  text-align: center;
  user-select: none;
}

.page-info {
  font-family: var(--font-mono, 'JetBrains Mono', monospace);
  font-size: var(--text-xs, 0.75rem);
  color: var(--color-text-tertiary, #9CA3AF);
  margin-left: var(--space-2, 8px);
}
</style>
```

- [ ] **Step 2: 提交**

```bash
cd c:/code/book
git add engineering/apps/daily-digest/web/src/components/Pagination.vue
git commit -m "feat: 重构分页组件，支持页码标签和每页条数选择器"
git push origin main
```

---

### Task 3: 美化 ItemCard（时间线节点 + 翻译按钮 + 标尺线）

**Files:**
- Modify: `engineering/apps/daily-digest/web/src/components/ItemCard.vue`

- [ ] **Step 1: 修改 ItemCard.vue**

在 `<template>` 中卡片底部新增翻译按钮区域（在 `card-footer` 下方）：

```vue
    <!-- 翻译按钮 -->
    <div class="card-translate">
      <button
        v-if="!translating"
        class="btn-translate"
        @click.stop="doTranslate"
      >
        {{ translated ? '🌐 原文' : '🌐 翻译' }}
      </button>
      <span v-else class="btn-translate loading">翻译中...</span>
    </div>
```

注意：在 `.item-card` 上添加 `@click.stop` 会阻止按钮点击冒泡，所以按钮上不需要额外 `@click.stop`。使用 `@click.stop` 在按钮上。

在 `<script setup>` 中添加翻译状态和逻辑：

```typescript
import { ref } from 'vue'
import { api } from '../api/client'

// 翻译相关
const translated = ref(false)
const translating = ref(false)
const originalTitle = ref('')
const originalSummary = ref('')

async function doTranslate() {
  if (translated.value) {
    // 切回原文
    props.item.title = originalTitle.value
    props.item.summary = originalSummary.value
    translated.value = false
    return
  }
  translating.value = true
  try {
    // 第一次翻译时保存原文
    if (!originalTitle.value) {
      originalTitle.value = props.item.title
      originalSummary.value = props.item.summary
    }
    const text = `${props.item.title}\n${props.item.summary}`
    const resp = await api.translate(text)
    const lines = resp.translated_text.split('\n')
    props.item.title = lines[0] || resp.translated_text
    props.item.summary = lines.slice(1).join('\n') || resp.translated_text
    translated.value = true
  } catch (e) {
    console.error('翻译失败:', e)
  } finally {
    translating.value = false
  }
}
```

在 `<style scoped>` 末尾添加：

```css
/* 翻译按钮 */
.card-translate {
  margin-top: var(--space-2, 8px);
  padding-top: var(--space-2, 8px);
  border-top: 1px solid var(--color-border-light, #EEF0F4);
}

.btn-translate {
  background: none;
  border: none;
  font-size: var(--text-xs, 0.75rem);
  color: var(--color-text-tertiary, #9CA3AF);
  cursor: pointer;
  padding: 2px 0;
  transition: color var(--transition-fast, 150ms);
  font-family: var(--font-body, sans-serif);
}

.btn-translate:hover {
  color: var(--color-brand, #2D7FF9);
}

.btn-translate.loading {
  cursor: wait;
  opacity: 0.6;
}

/* 节点圆点 — 时间线视觉效果 */
.item-card {
  position: relative;
}

.item-card::before {
  content: '';
  position: absolute;
  left: -28px;
  top: 24px;
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: var(--source-color, var(--color-brand, #2D7FF9));
  border: 2px solid var(--color-surface, #fff);
  z-index: 1;
  box-shadow: 0 0 0 2px var(--source-color, var(--color-brand, #2D7FF9));
}

/* 底部标尺线 — 加强分隔 */
.item-card::after {
  content: '';
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  height: 1px;
  background: color-mix(in srgb, var(--source-color, var(--color-brand, #2D7FF9)) 10%, transparent);
  border-radius: 0 0 var(--radius-lg, 12px) var(--radius-lg, 12px);
}

/* hover 增强 */
.item-card:hover {
  box-shadow: var(--shadow-card-hover, 0 4px 12px rgba(0,0,0,0.08));
  transform: translateY(-2px);
}

.item-card:hover .source-line-* {
  border-left-width: 6px;
}
```

同时需要在卡片上暴露来源色给 CSS。在 `item-card` 元素上增加 `:style`：

```vue
<article
  class="item-card"
  :class="[`source-line-${item.source}`]"
  :style="{ '--source-color': `var(--color-source-${item.source})` }"
  @click="$router.push(`/item/${item.id}`)"
>
```

- [ ] **Step 2: 在 api/client.ts 新增 translate 方法**

在 `api` 对象中添加：

```typescript
  translate(text: string) {
    return request<{ translated_text: string }>('/translate', {
      method: 'POST',
      body: JSON.stringify({ text }),
    })
  },
```

- [ ] **Step 3: 提交**

```bash
cd c:/code/book
git add engineering/apps/daily-digest/web/src/components/ItemCard.vue \
       engineering/apps/daily-digest/web/src/api/client.ts
git commit -m "feat: ItemCard 添加时间线节点、翻译按钮和底部标尺线"
git push origin main
```

---

### Task 4: 更新各页面 — DailyView / SearchView / HistoryView

**Files:**
- Modify: `engineering/apps/daily-digest/web/src/pages/DailyView.vue`
- Modify: `engineering/apps/daily-digest/web/src/pages/SearchView.vue`
- Modify: `engineering/apps/daily-digest/web/src/pages/HistoryView.vue`

- [ ] **Step 1: 修改 DailyView.vue**

改动点：
1. `pageSize` 改为响应式 `ref(10)`（默认 10）
2. 监听 `pageSize` 变化时重置 `page=1` 并重新加载
3. `.item-list` 添加时间线竖线样式
4. Pagination 组件传递 `v-model:size="pageSize"`

```vue
const pageSize = ref(10)

// 修改 loadItems 参数引用
async function loadItems() {
  loading.value = true
  try {
    const resp = await api.getDaily(
      activeCategory.value || undefined,
      page.value,
      pageSize.value
    )
    items.value = resp.items
    total.value = resp.total
  } catch (e) {
    console.error('加载失败:', e)
    items.value = []
    total.value = 0
  } finally {
    loading.value = false
  }
}

// 监听 pageSize 变化
watch(pageSize, () => {
  page.value = 1
  loadItems()
})

// 监听器加上 pageSize
watch([activeCategory, page, pageSize], () => {
  loadItems()
})
```

Pagination 组件绑定：
```vue
<Pagination
  v-model="page"
  v-model:size="pageSize"
  :total="total"
  :size="pageSize"
/>
```

在 `<style scoped>` 中为 `.item-list` 添加：
```css
.item-list {
  position: relative;
  display: flex;
  flex-direction: column;
  gap: var(--space-5, 20px);
  padding-left: 24px;
}

/* 时间线竖线 */
.item-list::before {
  content: '';
  position: absolute;
  left: 15px;
  top: 0;
  bottom: 0;
  width: 1px;
  border-left: 1px dashed var(--color-border-light, #EEF0F4);
}
```

- [ ] **Step 2: 修改 SearchView.vue**

同 DailyView 的三处改动：
1. `pageSize` 改为 `ref(10)`
2. Pagination 增加 `v-model:size="pageSize"`
3. `.item-list` 添加时间线竖线样式

```vue
const pageSize = ref(10)

// 监听 pageSize
watch(pageSize, () => {
  page.value = 1
  if (query.value.trim()) doSearch()
})
```

```vue
<Pagination
  v-model="page"
  v-model:size="pageSize"
  :total="total"
  :size="pageSize"
/>
```

```css
.item-list {
  position: relative;
  display: flex;
  flex-direction: column;
  gap: var(--space-5, 20px);
  padding-left: 24px;
}

.item-list::before {
  content: '';
  position: absolute;
  left: 15px;
  top: 0;
  bottom: 0;
  width: 1px;
  border-left: 1px dashed var(--color-border-light, #EEF0F4);
}
```

- [ ] **Step 3: 修改 HistoryView.vue**

同上述改动：
1. `pageSize` 改为 `ref(10)`
2. Pagination 增加 `v-model:size="pageSize"`
3. `.item-list` 添加时间线竖线样式

```vue
const pageSize = ref(10)

// 监听 pageSize
watch(pageSize, () => {
  page.value = 1
  loadHistory()
})
```

```vue
<Pagination
  v-model="page"
  v-model:size="pageSize"
  :total="total"
  :size="pageSize"
/>
```

```css
.item-list {
  position: relative;
  display: flex;
  flex-direction: column;
  gap: var(--space-5, 20px);
  padding-left: 24px;
}

.item-list::before {
  content: '';
  position: absolute;
  left: 15px;
  top: 0;
  bottom: 0;
  width: 1px;
  border-left: 1px dashed var(--color-border-light, #EEF0F4);
}
```

- [ ] **Step 4: 提交**

```bash
cd c:/code/book
git add engineering/apps/daily-digest/web/src/pages/DailyView.vue \
       engineering/apps/daily-digest/web/src/pages/SearchView.vue \
       engineering/apps/daily-digest/web/src/pages/HistoryView.vue
git commit -m "feat: 各页面适配响应式 pageSize 和时间线布局"
git push origin main
```

---

### Task 5: DetailView 翻译按钮

**Files:**
- Modify: `engineering/apps/daily-digest/web/src/pages/DetailView.vue`

- [ ] **Step 1: 在 DetailView 添加翻译按钮**

在 `detail-summary` 和 `detail-raw` 区域各加一个翻译按钮。

在 `<script setup>` 中添加：

```typescript
import { ref } from 'vue'
import { api } from '../api/client'

const summaryTranslated = ref(false)
const summaryTranslating = ref(false)
const originalSummary = ref('')
const originalRaw = ref('')
const rawTranslated = ref(false)
const rawTranslating = ref(false)

async function translateSummary() {
  if (summaryTranslated.value) {
    item.value!.summary = originalSummary.value
    summaryTranslated.value = false
    return
  }
  summaryTranslating.value = true
  try {
    if (!originalSummary.value) originalSummary.value = item.value!.summary
    const resp = await api.translate(item.value!.summary)
    item.value!.summary = resp.translated_text
    summaryTranslated.value = true
  } catch (e) {
    console.error('翻译失败:', e)
  } finally {
    summaryTranslating.value = false
  }
}

async function translateRaw() {
  if (rawTranslated.value) {
    item.value!.raw_content = originalRaw.value
    rawTranslated.value = false
    return
  }
  rawTranslating.value = true
  try {
    if (!originalRaw.value) originalRaw.value = item.value!.raw_content
    const resp = await api.translate(item.value!.raw_content)
    item.value!.raw_content = resp.translated_text
    rawTranslated.value = true
  } catch (e) {
    console.error('翻译失败:', e)
  } finally {
    rawTranslating.value = false
  }
}
```

在模板中，摘要和原文区域各添加翻译按钮：

```vue
      <div class="detail-summary" v-if="item.summary">
        <h3>摘要</h3>
        <p>{{ item.summary }}</p>
        <button
          class="btn btn-ghost btn-sm"
          @click="translateSummary"
          :disabled="summaryTranslating"
        >
          {{ summaryTranslating ? '翻译中...' : (summaryTranslated ? '🌐 原文' : '🌐 翻译') }}
        </button>
      </div>

      <div class="detail-raw" v-if="item.raw_content">
        <h3>原文内容</h3>
        <p>{{ item.raw_content }}</p>
        <button
          class="btn btn-ghost btn-sm"
          @click="translateRaw"
          :disabled="rawTranslating"
        >
          {{ rawTranslating ? '翻译中...' : (rawTranslated ? '🌐 原文' : '🌐 翻译') }}
        </button>
      </div>
```

- [ ] **Step 2: 提交**

```bash
cd c:/code/book
git add engineering/apps/daily-digest/web/src/pages/DetailView.vue
git commit -m "feat: DetailView 添加翻译按钮（摘要和原文）"
git push origin main
```

---

### Task 6: 端到端验证

- [ ] **Step 1: 启动后端**

```bash
cd c:/code/book/engineering/apps/daily-digest/backend
uvicorn app.main:app --reload
```

- [ ] **Step 2: 启动前端**

```bash
cd c:/code/book/engineering/apps/daily-digest/web
npm run dev
```

- [ ] **Step 3: 验证分页功能**

```bash
# 验证默认 10 条/页
curl -s "http://localhost:8000/api/v1/daily?page=1&size=10" | python -c "import sys,json; d=json.load(sys.stdin); print(f'items: {len(d[\"items\"])}, total: {d[\"total\"]}, page: {d[\"page\"]}, size: {d[\"size\"]}')"

# 验证分页正常
curl -s "http://localhost:8000/api/v1/daily?page=2&size=10" | python -c "import sys,json; d=json.load(sys.stdin); print(f'page 2: {len(d[\"items\"])} items')"

# 验证条数切换（50 条）
curl -s "http://localhost:8000/api/v1/daily?page=1&size=50" | python -c "import sys,json; d=json.load(sys.stdin); print(f'size=50: {len(d[\"items\"])} items')"
```

- [ ] **Step 4: 验证翻译接口**

```bash
curl -s -X POST http://localhost:8000/api/v1/translate \
  -H "Content-Type: application/json" \
  -d '{"text": "This is a test article about database performance optimization."}' | python -m json.tool
```

- [ ] **Step 5: 浏览器验证**

打开 http://localhost:3000，确认：
1. 分页栏显示页码标签（1 2 3 ... N）和每页条数下拉框
2. 卡片列表左侧有时间线竖线和节点圆点
3. 卡片之间有明显的分隔感
4. 点击「翻译」按钮触发翻译
5. 切换每页条数后分页正确重置到第 1 页
6. 不同主题（light/dark/eye/warm）颜色适配正常
