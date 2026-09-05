import type { Config } from 'tailwindcss';
import typography from '@tailwindcss/typography';

export default {
  content: [
    './index.html',
    './src/**/*.{ts,tsx}',
    '../../games/web/shared/web/src/**/*.{ts,tsx}',
  ],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        primary: {
          50: '#eef2ff', 100: '#e0e7ff', 300: '#a5b4fc', 600: '#4f46e5', 700: '#4338ca', 800: '#3730a3', 900: '#312e81',
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
  plugins: [typography],
} satisfies Config;
