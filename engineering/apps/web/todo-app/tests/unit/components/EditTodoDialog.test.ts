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

  it('preserves string labels verbatim (no silent data loss)', () => {
    const stringLabelsTodo: Todo = { ...baseTodo, labels: 'bug, urgent' }
    const wrapper = mount(EditTodoDialog, {
      props: { modelValue: true, todo: stringLabelsTodo, groups }
    })
    const input = wrapper.find('[data-test="edit-labels"]')
    expect((input.element as HTMLInputElement).value).toBe('bug, urgent')
  })

  it('joins array labels with ", "', () => {
    const wrapper = mountDialog()
    const input = wrapper.find('[data-test="edit-labels"]')
    expect((input.element as HTMLInputElement).value).toBe('urgent')
  })

  it('shows error and does not close when save fails', async () => {
    vi.mocked(todosApi.update).mockRejectedValue(new Error('网络错误'))
    const wrapper = mountDialog()
    await wrapper.find('[data-test="edit-save"]').trigger('click')
    await flushPromises()
    expect(wrapper.emitted('updated')).toBeFalsy()
    expect(wrapper.emitted('update:modelValue')).toBeFalsy()
    const errors = wrapper.emitted('error')
    expect(errors).toBeTruthy()
    expect(errors!.at(0)).toEqual([{ action: 'save', message: '网络错误' }])
    const errorEl = wrapper.find('[data-test="edit-error"]')
    expect(errorEl.exists()).toBe(true)
    expect(errorEl.text()).toBe('网络错误')
  })

  it('shows error and does not close when delete fails', async () => {
    vi.mocked(todosApi.remove).mockRejectedValue(new Error('删除失败'))
    const wrapper = mountDialog()
    await wrapper.find('[data-test="edit-delete"]').trigger('click')
    await flushPromises()
    expect(wrapper.emitted('deleted')).toBeFalsy()
    expect(wrapper.emitted('update:modelValue')).toBeFalsy()
    const errors = wrapper.emitted('error')
    expect(errors).toBeTruthy()
    expect(errors!.at(0)).toEqual([{ action: 'delete', message: '删除失败' }])
    const errorEl = wrapper.find('[data-test="edit-error"]')
    expect(errorEl.exists()).toBe(true)
    expect(errorEl.text()).toBe('删除失败')
  })

  it('clears the error message when the dialog is reopened with a new todo', async () => {
    vi.mocked(todosApi.update).mockRejectedValueOnce(new Error('失败一次'))
    const wrapper = mountDialog()
    await wrapper.find('[data-test="edit-save"]').trigger('click')
    await flushPromises()
    expect(wrapper.find('[data-test="edit-error"]').exists()).toBe(true)

    const fresh: Todo = { ...baseTodo, id: 99, title: 'Fresh' }
    await wrapper.setProps({ todo: fresh })
    expect(wrapper.find('[data-test="edit-error"]').exists()).toBe(false)
  })
})
