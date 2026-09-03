/**
 * 用户 Store
 *
 * 存储用户基本信息：昵称、头像首字母、邮箱、加入日期
 * 用于 TopBar 用户下拉菜单、设置页编辑资料
 */
import { create } from 'zustand'
import { persist } from 'zustand/middleware'

// 用户信息接口
export interface UserInfo {
  name: string         // 昵称
  avatar: string       // 头像首字母（自动从 name 派生，但允许手动覆盖）
  email?: string       // 邮箱（可选）
  joinDate: string     // 加入日期（ISO 格式）
  bio?: string         // 个人简介（可选）
}

// Store 接口
interface UserState {
  user: UserInfo
  setName: (name: string) => void
  setEmail: (email: string) => void
  setBio: (bio: string) => void
  setAvatar: (avatar: string) => void
  reset: () => void
}

// 工具函数：从姓名生成首字母
export function getInitial(name: string): string {
  if (!name || !name.trim()) return '?'
  const trimmed = name.trim()
  // 中英文混合：优先取中文第一个字，否则取英文首字母
  // 对于英文，取首字母并大写
  if (/^[a-zA-Z\s]/.test(trimmed)) {
    return trimmed.charAt(0).toUpperCase()
  }
  return trimmed.charAt(0)
}

// 默认用户
const defaultUser: UserInfo = {
  name: 'Yinch',
  avatar: 'Y',
  joinDate: new Date().toISOString(),
  bio: 'Reading Radar 学习者'
}

export const useUserStore = create<UserState>()(
  persist(
    (set, get) => ({
      user: defaultUser,

      // 修改昵称（同时更新 avatar 首字母，除非用户手动设过）
      setName: (name) => {
        const oldUser = get().user
        // 仅当 avatar 与旧 name 派生的首字母一致时，才同步更新
        const oldInitial = getInitial(oldUser.name)
        const newInitial = getInitial(name)
        const shouldUpdateAvatar = oldUser.avatar === oldInitial || !oldUser.avatar
        set({
          user: {
            ...oldUser,
            name,
            avatar: shouldUpdateAvatar ? newInitial : oldUser.avatar
          }
        })
      },

      // 修改邮箱
      setEmail: (email) => {
        set({ user: { ...get().user, email } })
      },

      // 修改简介
      setBio: (bio) => {
        set({ user: { ...get().user, bio } })
      },

      // 手动设置头像（首字母或 emoji）
      setAvatar: (avatar) => {
        set({ user: { ...get().user, avatar } })
      },

      // 重置用户（保留 joinDate）
      reset: () => {
        set({ user: { ...defaultUser, joinDate: get().user.joinDate } })
      }
    }),
    {
      name: 'rr2-user'
    }
  )
)
