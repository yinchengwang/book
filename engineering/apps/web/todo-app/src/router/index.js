import { createRouter, createWebHistory } from 'vue-router'
import TableView from '../views/TableView.vue'
import BoardView from '../views/BoardView.vue'
import StatsView from '../views/StatsView.vue'
import GroupManager from '../views/GroupManager.vue'
import CalendarView from '../views/CalendarView.vue'
import StatsDFXView from '../views/StatsDFXView.vue'
import PlanManageView from '../views/PlanManageView.vue'
import ViewManager from '../views/ViewManager.vue'
import GanttView from '../views/GanttView.vue'

const routes = [
  { path: '/', component: TableView, meta: { title: '待办列表' } },
  { path: '/board', component: BoardView, meta: { title: '看板视图' } },
  { path: '/stats', component: StatsView, meta: { title: '统计看板' } },
  { path: '/groups', component: GroupManager, meta: { title: '分组管理' } },
  { path: '/calendar', component: CalendarView, meta: { title: '日历' } },
  { path: '/stats-dfx', component: StatsDFXView, meta: { title: 'DFX 统计' } },
  { path: '/plan-manage', component: PlanManageView, meta: { title: '计划管理' } },
  { path: '/views', component: ViewManager, meta: { title: '视图管理' } },
  { path: '/gantt', component: GanttView, meta: { title: '甘特图' } }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

router.beforeEach((to) => {
  document.title = to.meta.title ? `${to.meta.title} - Todo App` : 'Todo App'
})

export default router
