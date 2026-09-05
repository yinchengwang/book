import { describe, it, expectTypeOf } from 'vitest'
import type { Todo, Group, ChecklistItem, Comment } from '@/types/models'
import type { ApiResponse, TodoListQuery } from '@/types/api'
import type { Theme, Filter, ToastMessage } from '@/types/ui'

describe('domain types', () => {
  it('Todo has expected shape', () => {
    expectTypeOf<Todo>().toHaveProperty('id')
    expectTypeOf<Todo>().toHaveProperty('title')
    expectTypeOf<Todo>().toHaveProperty('priority')
    expectTypeOf<Todo>().toHaveProperty('status')
  })

  it('Group has expected shape', () => {
    expectTypeOf<Group>().toHaveProperty('id')
    expectTypeOf<Group>().toHaveProperty('name')
  })

  it('ChecklistItem has expected shape', () => {
    expectTypeOf<ChecklistItem>().toHaveProperty('id')
    expectTypeOf<ChecklistItem>().toHaveProperty('text')
    expectTypeOf<ChecklistItem>().toHaveProperty('done')
  })

  it('Comment has expected shape', () => {
    expectTypeOf<Comment>().toHaveProperty('id')
    expectTypeOf<Comment>().toHaveProperty('text')
    expectTypeOf<Comment>().toHaveProperty('created_at')
  })

  it('ApiResponse generic envelopes data', () => {
    type R = ApiResponse<Todo[]>
    expectTypeOf<R['code']>().toEqualTypeOf<number>()
    expectTypeOf<R['data']>().toEqualTypeOf<Todo[] | undefined>()
    expectTypeOf<R['msg']>().toEqualTypeOf<string | undefined>()
  })

  it('TodoListQuery has filtering keys', () => {
    expectTypeOf<TodoListQuery>().toHaveProperty('status')
    expectTypeOf<TodoListQuery>().toHaveProperty('priority')
    expectTypeOf<TodoListQuery>().toHaveProperty('group_id')
    expectTypeOf<TodoListQuery>().toHaveProperty('search')
  })

  it('Theme is a union of literals', () => {
    expectTypeOf<Theme>().toEqualTypeOf<'light' | 'dark' | 'auto'>()
  })

  it('Filter has stable shape', () => {
    expectTypeOf<Filter['status']>().toEqualTypeOf<'all' | 'open' | 'closed' | 'archived'>()
    expectTypeOf<Filter['priority']>().toEqualTypeOf<number>()
    expectTypeOf<Filter['group_id']>().toEqualTypeOf<number>()
    expectTypeOf<Filter['search']>().toEqualTypeOf<string>()
  })

  it('ToastMessage has msg and type', () => {
    expectTypeOf<ToastMessage['msg']>().toEqualTypeOf<string>()
    expectTypeOf<ToastMessage['type']>().toEqualTypeOf<'success' | 'error' | 'info'>()
  })
})
