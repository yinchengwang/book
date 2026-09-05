import { describe, it, expect } from 'vitest'
import { mount } from '@vue/test-utils'
import InlineEditTitle from '@/components/todo/InlineEditTitle.vue'

describe('InlineEditTitle', () => {
  it('renders the title as text when not editing', () => {
    const wrapper = mount(InlineEditTitle, { props: { modelValue: 'Buy milk' } })
    expect(wrapper.text()).toContain('Buy milk')
    expect(wrapper.find('input').exists()).toBe(false)
  })

  it('switches to input on double click', async () => {
    const wrapper = mount(InlineEditTitle, { props: { modelValue: 'Buy milk' } })
    await wrapper.trigger('dblclick')
    const input = wrapper.find('input.inline-edit-input')
    expect(input.exists()).toBe(true)
    expect((input.element as HTMLInputElement).value).toBe('Buy milk')
  })

  it('emits update:modelValue on input', async () => {
    const wrapper = mount(InlineEditTitle, { props: { modelValue: 'Buy milk' } })
    await wrapper.trigger('dblclick')
    await wrapper.find('input.inline-edit-input').setValue('Buy bread')
    const events = wrapper.emitted('update:modelValue')
    expect(events).toBeTruthy()
    expect(events!.at(-1)).toEqual(['Buy bread'])
  })

  it('emits commit on Enter and exits edit mode', async () => {
    const wrapper = mount(InlineEditTitle, { props: { modelValue: 'Buy milk' } })
    await wrapper.trigger('dblclick')
    await wrapper.find('input.inline-edit-input').setValue('Buy bread')
    await wrapper.find('input.inline-edit-input').trigger('keyup.enter')
    const commits = wrapper.emitted('commit')
    expect(commits).toBeTruthy()
    expect(commits!.at(-1)).toEqual(['Buy bread'])
    expect(wrapper.find('input.inline-edit-input').exists()).toBe(false)
  })

  it('reverts to original on Escape', async () => {
    const wrapper = mount(InlineEditTitle, { props: { modelValue: 'Buy milk' } })
    await wrapper.trigger('dblclick')
    await wrapper.find('input.inline-edit-input').setValue('changed')
    await wrapper.find('input.inline-edit-input').trigger('keyup.esc')
    const updates = wrapper.emitted('update:modelValue')
    expect(updates!.at(-1)).toEqual(['Buy milk'])
    expect(wrapper.find('input.inline-edit-input').exists()).toBe(false)
  })

  it('does not commit when value is empty', async () => {
    const wrapper = mount(InlineEditTitle, { props: { modelValue: 'Buy milk' } })
    await wrapper.trigger('dblclick')
    await wrapper.find('input.inline-edit-input').setValue('   ')
    await wrapper.find('input.inline-edit-input').trigger('keyup.enter')
    const commits = wrapper.emitted('commit')
    expect(commits).toBeFalsy()
  })
})
