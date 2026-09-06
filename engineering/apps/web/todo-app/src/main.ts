import { createApp } from 'vue'
import { createPinia } from 'pinia'
import piniaPersist from 'pinia-plugin-persistedstate'
import App from './App.vue'
import router from './router'
import './styles/main.css'
import { useAuthStore } from '@/stores/auth'
import { useUIStore } from '@/stores/ui'
import { useTheme } from '@/composables/useTheme'

const pinia = createPinia()
pinia.use(piniaPersist)

const app = createApp(App)
app.use(pinia)
app.use(router)

// Initialize singletons (theme watcher registration, auth hydration)
useTheme()
const auth = useAuthStore()
auth.loadFromStorage()
const ui = useUIStore()
ui.resetFilter()

app.mount('#app')
