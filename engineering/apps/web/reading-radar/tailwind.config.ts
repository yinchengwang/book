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
        // radar / knowledge graph palette (placeholder for MVP-5)
        radar: {
          bg: '#f8fafc',
          node: '#6366f1',
          edge: '#cbd5e1',
        },
      },
      fontFamily: {
        sans: ['Inter', 'Noto Sans SC', 'system-ui', 'sans-serif'],
        mono: ['JetBrains Mono', 'Cascadia Code', 'monospace'],
      },
    },
  },
  plugins: [],
} satisfies Config;
