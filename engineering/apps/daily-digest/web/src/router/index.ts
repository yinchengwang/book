import { createRouter, createWebHistory } from 'vue-router'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/',
      name: 'daily',
      component: () => import('../pages/DailyView.vue'),
      meta: { title: '每日速览' },
    },
    {
      path: '/item/:id',
      name: 'detail',
      component: () => import('../pages/DetailView.vue'),
      meta: { title: '详情' },
    },
    {
      path: '/search',
      name: 'search',
      component: () => import('../pages/SearchView.vue'),
      meta: { title: '搜索' },
    },
    {
      path: '/collections',
      name: 'collections',
      component: () => import('../pages/CollectionsView.vue'),
      meta: { title: '收藏夹' },
    },
    {
      path: '/subscriptions',
      name: 'subscriptions',
      component: () => import('../pages/SubscriptionsView.vue'),
      meta: { title: '订阅管理' },
    },
    {
      path: '/history',
      name: 'history',
      component: () => import('../pages/HistoryView.vue'),
      meta: { title: '阅读历史' },
    },
    {
      path: '/settings',
      name: 'settings',
      component: () => import('../pages/SettingsView.vue'),
      meta: { title: '设置' },
    },
  ],
})

router.afterEach((to) => {
  document.title = `${to.meta.title || '每日速览'} — DailyDigest`
})

export default router
