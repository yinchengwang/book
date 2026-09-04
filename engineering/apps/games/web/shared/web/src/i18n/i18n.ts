// shared/web/src/i18n/i18n.ts
import i18n from 'i18next';
import { initReactI18next } from 'react-i18next';

i18n.use(initReactI18next).init({
  resources: {
    zh: {
      translation: {
        play: '开始游戏',
        restart: '重新开始',
        score: '分数',
        high: '最高分',
        newGame: '新游戏',
        undo: '撤销',
        theme: '主题',
        language: '语言',
        back: '返回首页',
        gameOver: '游戏结束',
        won: '恭喜达成 2048！',
        easy: '简单',
        medium: '中等',
        hard: '困难',
      },
    },
    en: {
      translation: {
        play: 'Play',
        restart: 'Restart',
        score: 'Score',
        high: 'High',
        newGame: 'New Game',
        undo: 'Undo',
        theme: 'Theme',
        language: 'Language',
        back: 'Back',
        gameOver: 'Game Over',
        won: 'You Win!',
        easy: 'Easy',
        medium: 'Medium',
        hard: 'Hard',
      },
    },
  },
  lng: typeof localStorage !== 'undefined'
    ? (localStorage.getItem('lang') ?? 'zh')
    : 'zh',
  fallbackLng: 'zh',
  interpolation: { escapeValue: false },
});

export default i18n;
