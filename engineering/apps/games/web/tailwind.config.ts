import type { Config } from 'tailwindcss';

export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        primary: {
          50: '#eef2ff', 500: '#6366f1', 900: '#312e81',
        },
        g2048: {
          bg: '#faf8ef',
          tile: {
            2: '#eee4da', 4: '#ede0c8', 8: '#f2b179', 16: '#f59563',
            32: '#f67c5f', 64: '#f65e3b', 128: '#edcf72', 256: '#edcc61',
            512: '#edc850', 1024: '#edc53f', 2048: '#edc22e',
          },
        },
        snake: { board: '#f5f5f5', snake: '#2ecc71', food: '#e74c3c' },
        sudoku: { grid: '#bbada0', cell: '#faf8ef', accent: '#3b82f6' },
      },
      fontFamily: {
        sans: ['Inter', 'Noto Sans SC', 'system-ui', 'sans-serif'],
        mono: ['JetBrains Mono', 'Cascadia Code', 'monospace'],
      },
    },
  },
  plugins: [],
} satisfies Config;
