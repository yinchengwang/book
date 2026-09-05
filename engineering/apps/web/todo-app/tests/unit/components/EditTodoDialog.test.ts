import { describe, it, expect, beforeEach, vi } from 'vitest'
import { mount, flushPromises } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import EditTodoDialog from '@/components/todo/EditTodoDialog.vue'
import { useTodosStore } from '@/stores/todos'
import type { Todo, Group } from '@/types/models'
import type { ApiResponse } from '@/types/api'
import * as todosApi from '@/api/todos'

vi.mock('@/api/todos', () => ({
  list: vi.fn(),
  get: vi.fn(),
  create: vi.fn(),
  update: vi.fn(),
  remove: vi.fn(),
  updateSort: vi.fn(),
  reorder: vi.fn()
}))

const baseTodo: Todo = {
  id: 7,
  title: 'Original',
  description: 'desc',
  status: 'open',
  priority: 2,
  due_date: 0,
  group_id: 0,
  labels: ['urgent']
}

const groups: Group[] = [
  { id: 1, name: 'Work' },
  { id: 2, name: 'Home' }
]

function mountDialog() {
  const store = useTodosStore()
  store.todos = [{ ...baseTodo }]
  return mount(EditTodoDialog, {
    props: { modelValue: true, todo: { ...baseTodo }, groups }
  })
}

describe('EditTodoDialog', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    vi.clearAllMocks()
  })

  it('renders nothing when todo is null', () => {
    const wrapper = mount(EditTodoDialog, {
      props: { modelValue: true, todo: null, groups }
    })
    expect(wrapper.find('[data-test="edit-todo-dialog"]').exists()).toBe(false)
  })

  it('renders title input with current value', () => {
    const wrapper = mountDialog()
    const input = wrapper.find('[data-test="edit-title"]')
    expect(input.exists()).toBe(true)
    expect((input.element as HTMLInputElement).value).toBe('Original')
  })

  it('emits update:modelValue false on cancel', async () => {
    const wrapper = mountDialog()
    await wrapper.find('[data-test="edit-cancel"]').trigger('click')
    expect(wrapper.emitted('update:modelValue')?.at(-1)).toEqual([false])
  })

  it('updates todo and emits updated on save', async () => {
    vi.mocked(todosApi.update).mockResolvedValue({
      code: 0,
      data: { ...baseTodo, title: 'Updated' }
    } as ApiResponse<Todo>)
    const wrapper = mountDialog()
    await wrapper.find('[data-test="edit-title"]').setValue('Updated')
    await wrapper.find('[data-test="edit-save"]').trigger('click')
    await flushPromises()
    expect(wrapper.emitted('updated')).toBeTruthy()
    expect(wrapper.emitted('updated')!.at(0)).toEqual([
      expect.objectContaining({ title: 'Updated' })
    ])
    const store = useTodosStore()
    expect(store.todos[0]?.title).toBe('Updated')
  })

  it('disables save when title is empty', async () => {
    const wrapper = mountDialog()
    await wrapper.find('[data-test="edit-title"]').setValue('   ')
    const save = wrapper.find('[data-test="edit-save"]')
    expect((save.element as HTMLButtonElement).disabled).toBe(true)
  })

  it('emits deleted when delete button is clicked', async () => {
    vi.mocked(todosApi.remove).mockResolvedValue({ code: 0 } as ApiResponse<null>)
    const wrapper = mountDialog()
    await wrapper.find('[data-test="edit-delete"]').trigger('click')
    await flushPromises()
    expect(wrapper.emitted('deleted')).toBeTruthy()
    expect(wrapper.emitted('deleted')!.at(-1)).toEqual([7])
  })

  it('parses labels comma-separated input', async () => {
    vi.mocked(todosApi.update).mockResolvedValue({
      code: 0,
      data: { ...baseTodo, labels: ['a', 'b'] }
    } as ApiResponse<Todo>)
    const wrapper = mountDialog()
    await wrapper.find('[data-test="edit-labels"]').setValue('a, b ,c')
    await wrapper.find('[data-test="edit-save"]').trigger('click')
    await flushPromises()
    const call = vi.mocked(todosApi.update).mock.calls.at(-1)
    expect(call?.[1]?.labels).toEqual(['a', 'b', 'c'])
  })
})
