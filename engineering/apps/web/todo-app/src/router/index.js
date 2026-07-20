import { createRouter, createWebHistory } from 'vue-router'
import ListView from '../views/ListView.vue'
import BoardView from '../views/BoardView.vue'
import StatsView from '../views/StatsView.vue'
import GroupManager from '../views/GroupManager.vue'
import CalendarView from '../views/CalendarView.vue'
import StatsDFXView from '../views/StatsDFXView.vue'
import PlanManageView from '../views/PlanManageView.vue'

const routes = [
  { path: '/', component: ListView, meta: { title: '待办列表' } },
  { path: '/board', component: BoardView, meta: { title: '看板视图' } },
  { path: '/stats', component: StatsView, meta: { title: '统计看板' } },
  { path: '/groups', component: GroupManager, meta: { title: '分组管理' } },
  { path: '/calendar', component: CalendarView, meta: { title: '日历' } },
  { path: '/stats-dfx', component: StatsDFXView, meta: { title: 'DFX 统计' } },
  { path: '/plan-manage', component: PlanManageView, meta: { title: '计划管理' } }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

router.beforeEach((to) => {
  document.title = to.meta.title ? `${to.meta.title} - Todo App` : 'Todo App'
})

export default router
