import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'
import { ifdefPlugin, ifdefEsbuildPlugin } from './vite-plugins/ifdef'

// H5 端 Vite 配置
// - 保留 src/ 源码不动
// - alias 映射 Taro 组件到 web 适配层
// - 路径别名 @/* 指向 src/*
export default defineConfig({
  plugins: [ifdefPlugin(), react()],

  esbuild: {
    legalComments: 'none'
  },

  resolve: {
    alias: {
      // Taro 组件 → 适配层
      '@tarojs/components': path.resolve(__dirname, 'adapters/index.ts'),
      '@tarojs/taro': path.resolve(__dirname, 'adapters/taro-shim.ts'),
      '@tarojs/runtime': path.resolve(__dirname, 'adapters/taro-shim.ts'),

      // H5 端专用：覆盖小程序版 TabBar
      '@/components/TabBar/TabBar': path.resolve(__dirname, 'adapters/TabBarH5.tsx'),

      // 路径别名（与 config/index.js 同步）
      '@': path.resolve(__dirname, '../src'),
      '@/components': path.resolve(__dirname, '../src/components'),
      '@/pages': path.resolve(__dirname, '../src/pages'),
      '@/stores': path.resolve(__dirname, '../src/stores'),
      '@/data': path.resolve(__dirname, '../src/data'),
      '@/utils': path.resolve(__dirname, '../src/utils'),
      '@/styles': path.resolve(__dirname, '../src/styles'),
      '@/hooks': path.resolve(__dirname, '../src/hooks')
    }
  },

  server: {
    host: '0.0.0.0',
    port: 5173,
    open: true,
    hmr: {
      overlay: false
    }
  },

  build: {
    outDir: 'dist',
    sourcemap: true,
    chunkSizeWarningLimit: 1500,
    rollupOptions: {
      output: {
        manualChunks: {
          'react-vendor': ['react', 'react-dom', 'react-router-dom'],
          'charts': ['recharts'],
          'graph': ['@xyflow/react'],
          'dnd': ['@dnd-kit/core', '@dnd-kit/sortable']
        }
      }
    }
  },

  optimizeDeps: {
    include: ['react', 'react-dom', 'react-router-dom', 'zustand', 'recharts', 'lucide-react']
  }
})
