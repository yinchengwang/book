import { create } from 'zustand'
import { persist } from 'zustand/middleware'

// API 配置接口
export interface ApiConfig {
  apiKey: string
  apiUrl: string
  model: string
}

// 通知配置接口
export interface NotificationConfig {
  reviewReminder: boolean
  interviewReminder: boolean
}

// 设置状态接口
interface SettingsState {
  theme: 'light' | 'dark'
  apiConfig: ApiConfig
  notifications: NotificationConfig
  setTheme: (theme: 'light' | 'dark') => void
  setApiConfig: (config: ApiConfig) => void
  setNotifications: (config: NotificationConfig) => void
  exportData: () => void
  importData: () => void
  clearData: () => void
}

export const useSettingsStore = create<SettingsState>()(
  persist(
    (set) => ({
      // 默认主题
      theme: 'dark',

      // 默认 API 配置
      apiConfig: {
        apiKey: '',
        apiUrl: 'https://api.openai.com/v1',
        model: 'gpt-4'
      },

      // 默认通知配置
      notifications: {
        reviewReminder: true,
        interviewReminder: true
      },

      // 设置主题
      setTheme: (theme) => set({ theme }),

      // 设置 API 配置
      setApiConfig: (apiConfig) => set({ apiConfig }),

      // 设置通知配置
      setNotifications: (notifications) => set({ notifications }),

      // 导出数据
      exportData: () => {
        const data = {
          theme: localStorage.getItem('theme-storage'),
          review: localStorage.getItem('review-storage'),
          knowledge: localStorage.getItem('knowledge-storage'),
          quiz: localStorage.getItem('quiz-storage'),
          timestamp: new Date().toISOString()
        }
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' })
        const url = URL.createObjectURL(blob)
        const a = document.createElement('a')
        a.href = url
        a.download = `reading-radar-backup-${new Date().toISOString().split('T')[0]}.json`
        a.click()
        URL.revokeObjectURL(url)
      },

      // 导入数据
      importData: () => {
        const input = document.createElement('input')
        input.type = 'file'
        input.accept = '.json'
        input.onchange = (e) => {
          const file = (e.target as HTMLInputElement).files?.[0]
          if (file) {
            const reader = new FileReader()
            reader.onload = (event) => {
              try {
                const data = JSON.parse(event.target?.result as string)
                if (data.theme) localStorage.setItem('theme-storage', data.theme)
                if (data.review) localStorage.setItem('review-storage', data.review)
                if (data.knowledge) localStorage.setItem('knowledge-storage', data.knowledge)
                if (data.quiz) localStorage.setItem('quiz-storage', data.quiz)
                window.location.reload()
              } catch (err) {
                console.error('导入失败:', err)
                alert('导入失败，请检查文件格式')
              }
            }
            reader.readAsText(file)
          }
        }
        input.click()
      },

      // 清空数据
      clearData: () => {
        if (confirm('确定要清空所有数据吗？此操作不可恢复！')) {
          localStorage.removeItem('theme-storage')
          localStorage.removeItem('review-storage')
          localStorage.removeItem('knowledge-storage')
          localStorage.removeItem('quiz-storage')
          window.location.reload()
        }
      }
    }),
    {
      name: 'settings-storage'
    }
  )
)
