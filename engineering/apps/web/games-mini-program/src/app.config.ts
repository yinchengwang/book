export default defineAppConfig({
  pages: [
    'pages/index/index',
    'pages/snake/index',
    'pages/sudoku/index',
    'pages/game2048/index',
    'pages/match3/index'
  ],
  window: {
    navigationBarTitleText: '游戏中心',
    navigationBarBackgroundColor: '#1a1a2e',
    navigationBarTextStyle: 'white',
    backgroundColor: '#16213e',
    backgroundTextStyle: 'dark'
  },
  style: 'v2',
  sitemapLocation: 'sitemap.json'
})
