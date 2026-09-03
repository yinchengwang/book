export default defineAppConfig({
  pages: [
    'pages/index',
    'pages/review/Review',
    'pages/knowledge_graph/knowledge_graph',
    'pages/gap_analysis/gap_analysis',
    'pages/interview_tracker/interview_tracker',
    'pages/learning_path/learning_path',
    'pages/settings/Settings',
    'pages/excerpt/Excerpt',
    'pages/mock_interview/mock_interview',
    'pages/project_roadmap/project_roadmap',
    'pages/not-found/NotFound',
  ],
  window: {
    backgroundTextStyle: 'light',
    navigationBarBackgroundColor: '#0f172a',
    navigationBarTitleText: 'Knowledge Hub',
    navigationBarTextStyle: 'white'
  },
  // 小程序 TabBar 配置
  tabBar: {
    color: '#94a3b8',
    selectedColor: '#6366f1',
    backgroundColor: '#0f172a',
    borderStyle: 'black',
    list: [
      {
        pagePath: 'pages/index',
        text: '首页',
        iconPath: 'assets/tab-home.png',
        selectedIconPath: 'assets/tab-home-active.png'
      },
      {
        pagePath: 'pages/review/Review',
        text: '复习',
        iconPath: 'assets/tab-review.png',
        selectedIconPath: 'assets/tab-review-active.png'
      },
      {
        pagePath: 'pages/interview_tracker/interview_tracker',
        text: '面试',
        iconPath: 'assets/tab-tracker.png',
        selectedIconPath: 'assets/tab-tracker-active.png'
      },
      {
        pagePath: 'pages/learning_path/learning_path',
        text: '路径',
        iconPath: 'assets/tab-path.png',
        selectedIconPath: 'assets/tab-path-active.png'
      },
      {
        pagePath: 'pages/settings/Settings',
        text: '设置',
        iconPath: 'assets/tab-settings.png',
        selectedIconPath: 'assets/tab-settings-active.png'
      }
    ]
  }
})
