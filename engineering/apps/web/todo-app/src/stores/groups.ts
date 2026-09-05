import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import * as api from '@/api/groups'
import type { Group } from '@/types/models'
import type { CreateGroupInput } from '@/types/api'

export const useGroupsStore = defineStore('groups', () => {
  const groups = ref<Group[]>([])
  const loading = ref(false)
  const error = ref<Error | null>(null)

  const byId = computed(() => {
    const map = new Map<number, Group>()
    for (const g of groups.value) {
      map.set(g.id, g)
    }
    return (id: number) => map.get(id)
  })

  async function fetch(): Promise<void> {
    loading.value = true
    error.value = null
    try {
      const res = await api.list()
      if (res.code === 0 && res.data) {
        groups.value = res.data
      } else {
        error.value = new Error(res.msg ?? 'Failed to load groups')
      }
    } catch (e) {
      error.value = e as Error
    } finally {
      loading.value = false
    }
  }

  async function create(input: CreateGroupInput): Promise<Group> {
    const res = await api.create(input)
    if (res.code !== 0 || !res.data) throw new Error(res.msg ?? 'Create failed')
    groups.value.push(res.data)
    return res.data
  }

  async function update(id: number, input: Partial<CreateGroupInput>): Promise<Group> {
    const res = await api.update(id, input)
    if (res.code !== 0 || !res.data) throw new Error(res.msg ?? 'Update failed')
    const idx = groups.value.findIndex((g) => g.id === id)
    if (idx >= 0) groups.value[idx] = res.data
    return res.data
  }

  async function remove(id: number): Promise<void> {
    const res = await api.remove(id)
    if (res.code !== 0) throw new Error(res.msg ?? 'Remove failed')
    groups.value = groups.value.filter((g) => g.id !== id)
  }

  return {
    groups,
    loading,
    error,
    byId,
    fetch,
    create,
    update,
    remove
  }
})
