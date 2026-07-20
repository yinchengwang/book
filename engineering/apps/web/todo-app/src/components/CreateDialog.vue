<template>
  <div v-if="show" class="modal-mask" @click.self="show = false">
    <div class="modal">
      <h3>新建待办</h3>
      <label class="label-text">标题 *</label>
      <input v-model.trim="form.title" class="form-input" maxlength="255" @keyup.enter="submit" />
      <label class="label-text">描述</label>
      <textarea v-model="form.description" class="form-textarea" rows="4" maxlength="4000"></textarea>
      <label class="label-text">优先级</label>
      <select v-model="form.priority" class="form-select">
        <option v-for="(l, i) in PRIORITY_LABELS" :key="i" :value="i">{{ l }}</option>
      </select>
      <label class="label-text">截止日期（时间戳，0=无）</label>
      <input v-model.number="form.due_date" type="number" class="form-input" placeholder="0" />
      <label class="label-text">分组</label>
      <select v-model.number="form.group_id" class="form-select">
        <option :value="0">未分组</option>
        <option v-for="g in groups" :key="g.id" :value="g.id">{{ g.name }}</option>
      </select>
      <label class="label-text">标签（逗号分隔）</label>
      <input v-model="form.labels" class="form-input" placeholder="bug, urgent" />

      <!-- 扩展字段 -->
      <div v-if="extFields.length > 0" class="ext-fields-section">
        <h4 class="section-title">扩展字段</h4>
        <div v-for="field in extFields" :key="field.id" class="ext-field">
          <label class="label-text">{{ field.name }}</label>
          <input
            v-if="isTextType(field.type)"
            v-model="extValues[field.id]"
            class="form-input"
            :placeholder="'输入' + field.name"
          />
          <select
            v-else-if="field.type === 'single_select'"
            v-model="extValues[field.id]"
            class="form-select"
          >
            <option value="">请选择</option>
            <option
              v-for="choice in getSelectChoices(field)"
              :key="choice"
              :value="choice"
            >{{ choice }}</option>
          </select>
          <input
            v-else-if="field.type === 'number'"
            v-model.number="extValues[field.id]"
            type="number"
            class="form-input"
            :placeholder="'输入数字'"
          />
          <input
            v-else-if="field.type === 'date' || field.type === 'datetime'"
            v-model="extValues[field.id]"
            type="date"
            class="form-input"
          />
          <input
            v-else
            v-model="extValues[field.id]"
            class="form-input"
            :placeholder="'输入' + field.name"
          />
        </div>
      </div>

      <div style="display:flex; gap:8px; margin-top:16px;">
        <button class="btn btn-primary" :disabled="!form.title" @click="submit">创建</button>
        <button class="btn btn-secondary" @click="show = false">取消</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { reactive, ref, watch, onMounted } from 'vue'
import api from '../api.js'

const props = defineProps({ groups: Array })
const emit = defineEmits(['created'])

const show = defineModel({ type: Boolean, default: false })
const PRIORITY_LABELS = ['🔴紧急', '🟡高', '🔵中', '🟢低', '⚪无']

const form = reactive({ title: '', description: '', priority: 4, due_date: 0, group_id: 0, labels: '' })
const extFields = ref([])
const extValues = reactive({})

// 加载扩展字段
async function loadFields() {
  const res = await api.listFields()
  if (res.code === 0) {
    extFields.value = res.data.filter(f => !f.built_in)
  }
}

watch(show, (v) => {
  if (v) {
    form.title = ''; form.description = ''; form.priority = 4
    form.due_date = 0; form.group_id = 0; form.labels = ''
    Object.keys(extValues).forEach(k => delete extValues[k])
    loadFields()
  }
})

function isTextType(type) {
  return type === 'text' || type === 'multi_select'
}

function getSelectChoices(field) {
  try {
    const opts = typeof field.options === 'string' ? JSON.parse(field.options) : field.options
    return opts?.choices || []
  } catch { return [] }
}

function submit() {
  if (!form.title) return
  const labels = form.labels ? form.labels.split(',').map(s => s.trim()).filter(Boolean) : []
  const body = { ...form, labels }

  // 收集扩展字段值
  const fields = {}
  for (const field of extFields.value) {
    const val = extValues[field.id]
    if (val !== undefined && val !== '' && val !== null) {
      fields[String(field.id)] = String(val)
    }
  }

  emit('created', { body, fields })
  show.value = false
}

onMounted(loadFields)
</script>