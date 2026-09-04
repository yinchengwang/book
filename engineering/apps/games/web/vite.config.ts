import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
      '@shared': path.resolve(__dirname, './shared/web/src'), // shared code inside web/ (MVP-1); cross-project sharing deferred
    },
  },
  server: {
    port: 5173,
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          'g2048': ['./src/games/g2048'],
          'snake': ['./src/games/snake'],
          'sudoku': ['./src/games/sudoku'],
        },
      },
    },
  },
});
