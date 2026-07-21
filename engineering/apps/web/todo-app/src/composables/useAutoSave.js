/**
 * useAutoSave — 自动保存 composable
 *
 * 提供 debounce 延迟保存 + 变更队列管理，替代即时 API 调用。
 * 支持合并相同 (todoId, fieldId) 的变更，按 todoId 分组批量提交。
 */

import { ref, onUnmounted } from 'vue'
import api from '../api.js'

// 内置字段 ID → API 字段名映射
const FIELD_ID_MAP = {
  1: 'title',
  3: 'status',
  4: 'priority',
  5: 'due_date',
  7: 'group_id'
}

/**
 * @param {Object} options
 * @param {number} options.delay - debounce 延迟毫秒数，默认 800
 * @param {Function} options.onFlush - 每次 flush 后的回调（用于刷新数据）
 */
export function useAutoSave(options = {}) {
  const { delay = 800, onFlush } = options
  const pendingChanges = ref([])
  const saveStatus = ref('idle')  // idle | saving | saved | error
  let timer = null
  let retryCount = 0
  const MAX_RETRY = 3

  /**
   * 提交变更到队列（带 debounce 合并）
   * @param {number} todoId
   * @param {number} fieldId - 内置字段 1-9，扩展字段 >=10
   * @param {*} value
   */
  function scheduleSave(todoId, fieldId, value) {
    const existing = pendingChanges.value.find(
      c => c.todoId === todoId && c.fieldId === fieldId
    )
    if (existing) {
      existing.value = value
      existing.timestamp = Date.now()
    } else {
      pendingChanges.value.push({
        id: `${todoId}_${fieldId}`,
        todoId,
        fieldId,
        value,
        status: 'pending',
        timestamp: Date.now()
      })
    }
    debounceFlush()
  }

  function debounceFlush() {
    clearTimeout(timer)
    timer = setTimeout(flushChanges, delay)
  }

  async function flushChanges() {
    if (pendingChanges.value.length === 0) {
      saveStatus.value = 'idle'
      return
    }
    saveStatus.value = 'saving'

    const batch = pendingChanges.value.splice(0)
    try {
      // 按 todoId 分组
      const byTodo = new Map()
      for (const ch of batch) {
        if (!byTodo.has(ch.todoId)) {
          byTodo.set(ch.todoId, { todoFields: {}, todoUpdates: {} })
        }
        const group = byTodo.get(ch.todoId)
        if (ch.fieldId >= 10) {
          group.todoFields[String(ch.fieldId)] = String(ch.value)
        } else {
          const fieldName = FIELD_ID_MAP[ch.fieldId]
          if (fieldName) {
            group.todoUpdates[fieldName] = ch.value
          }
        }
      }

      // 并发提交
      await Promise.all(Array.from(byTodo.entries()).map(async ([todoId, changes]) => {
        if (Object.keys(changes.todoFields).length > 0) {
          await api.updateTodoFields(todoId, changes.todoFields)
        }
        if (Object.keys(changes.todoUpdates).length > 0) {
          // 按字段类型转换
          if (changes.todoUpdates.priority !== undefined) {
            changes.todoUpdates.priority = parseInt(changes.todoUpdates.priority)
          }
          if (changes.todoUpdates.due_date !== undefined) {
            changes.todoUpdates.due_date = parseInt(changes.todoUpdates.due_date) || 0
          }
          await api.update(todoId, changes.todoUpdates)
        }
      }))

      saveStatus.value = 'saved'
      retryCount = 0
      // 自动重置状态
      setTimeout(() => {
        if (saveStatus.value === 'saved') saveStatus.value = 'idle'
      }, 2000)

      // 回调通知
      if (onFlush) onFlush()
    } catch (e) {
      retryCount++
      if (retryCount <= MAX_RETRY) {
        // 重新入队，稍后重试
        pendingChanges.value.push(...batch)
        setTimeout(debounceFlush, 1000 * retryCount)
        saveStatus.value = 'saving'
      } else {
        saveStatus.value = 'error'
        retryCount = 0
        setTimeout(() => {
          if (saveStatus.value === 'error') saveStatus.value = 'idle'
        }, 5000)
      }
    }
  }

  /** 强制立即刷新（用于页面离开/视图切换前） */
  function forceFlush() {
    clearTimeout(timer)
    return flushChanges()
  }

  /** 是否有未保存的变更 */
  function hasPending() {
    return pendingChanges.value.length > 0
  }

  onUnmounted(() => {
    clearTimeout(timer)
  })

  return {
    pendingChanges,
    saveStatus,
    scheduleSave,
    forceFlush,
    hasPending
  }
}