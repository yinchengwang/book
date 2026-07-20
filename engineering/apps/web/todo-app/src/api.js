const BASE = '/api'

const api = {
  /* Todo CRUD */
  list: (params) => fetch(`${BASE}/todos?` + new URLSearchParams(params)).then(r => r.json()),
  get: (id) => fetch(`${BASE}/todos/${id}`).then(r => r.json()),
  create: (body) => fetch(`${BASE}/todos`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  update: (id, body) => fetch(`${BASE}/todos/${id}`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  remove: (id) => fetch(`${BASE}/todos/${id}`, { method: 'DELETE' }).then(r => r.json()),
  updateSort: (id, sort_order) => fetch(`${BASE}/todos/${id}/sort`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ sort_order }) }).then(r => r.json()),

  /* 统计 */
  stats: () => fetch(`${BASE}/todos/stats`).then(r => r.json()),

  /* Checklist */
  addChecklist: (id, text) => fetch(`${BASE}/todos/${id}/checklist`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ text }) }).then(r => r.json()),
  toggleChecklist: (id, itemId) => fetch(`${BASE}/todos/${id}/checklist/${itemId}`, { method: 'PATCH' }).then(r => r.json()),
  removeChecklist: (id, itemId) => fetch(`${BASE}/todos/${id}/checklist/${itemId}`, { method: 'DELETE' }).then(r => r.json()),

  /* 评论 */
  listComments: (id) => fetch(`${BASE}/todos/${id}/comments`).then(r => r.json()),
  addComment: (id, text) => fetch(`${BASE}/todos/${id}/comments`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ text }) }).then(r => r.json()),
  deleteComment: (id, cid) => fetch(`${BASE}/todos/${id}/comments/${cid}`, { method: 'DELETE' }).then(r => r.json()),

  /* 分组 */
  listGroups: () => fetch(`${BASE}/groups`).then(r => r.json()),
  createGroup: (body) => fetch(`${BASE}/groups`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  updateGroup: (id, body) => fetch(`${BASE}/groups/${id}`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  deleteGroup: (id) => fetch(`${BASE}/groups/${id}`, { method: 'DELETE' }).then(r => r.json()),

  /* OPSX 变更 */
  createChange: (id) => fetch(`${BASE}/todos/${id}/create-change`, { method: 'POST' }).then(r => r.json()),

  /* 日历 */
  calendarDay: (date, tsId) => fetch(`${BASE}/calendar/day?date=${date}&task_system_id=${tsId||-1}`).then(r => r.json()),
  calendarWeek: (date, tsId) => fetch(`${BASE}/calendar/week?date=${date}&task_system_id=${tsId||-1}`).then(r => r.json()),
  calendarMonth: (date, tsId) => fetch(`${BASE}/calendar/month?date=${date}&task_system_id=${tsId||-1}`).then(r => r.json()),
  pendingCarryover: () => fetch(`${BASE}/calendar/pending-carryover`).then(r => r.json()),
  confirmCarryover: (items) => fetch(`${BASE}/calendar/carryover-confirm`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(items) }).then(r => r.json()),

  /* DFX 统计 */
  statsDfx: () => fetch(`${BASE}/stats-dfx`).then(r => r.json()),
  statsHeatmap: () => fetch(`${BASE}/stats-dfx/heatmap`).then(r => r.json()),

  /* 任务系统 */
  listTaskSystems: () => fetch(`${BASE}/task-systems`).then(r => r.json()),
  createTaskSystem: (body) => fetch(`${BASE}/task-systems`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  updateTaskSystem: (id, body) => fetch(`${BASE}/task-systems/${id}`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  deleteTaskSystem: (id) => fetch(`${BASE}/task-systems/${id}`, { method: 'DELETE' }).then(r => r.json()),

  /* 学习计划 */
  listPlans: () => fetch(`${BASE}/plans`).then(r => r.json()),
  getPlan: (id) => fetch(`${BASE}/plans/${id}`).then(r => r.json()),
  createPlan: (body) => fetch(`${BASE}/plans`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  importTemplate: (body) => fetch(`${BASE}/plans/import-template`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  expandPlan: (id, body) => fetch(`${BASE}/plans/${id}/expand`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),

  /* 视图系统 */
  listViews: () => fetch(`${BASE}/views`).then(r => r.json()),
  createView: (body) => fetch(`${BASE}/views`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  getView: (id) => fetch(`${BASE}/views/${id}`).then(r => r.json()),
  updateView: (id, body) => fetch(`${BASE}/views/${id}`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  deleteView: (id) => fetch(`${BASE}/views/${id}`, { method: 'DELETE' }).then(r => r.json()),
  setViewDefault: (id) => fetch(`${BASE}/views/${id}/default`, { method: 'PATCH' }).then(r => r.json()),

  /* 字段系统 */
  listFields: () => fetch(`${BASE}/fields`).then(r => r.json()),
  createField: (body) => fetch(`${BASE}/fields`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  updateField: (id, body) => fetch(`${BASE}/fields/${id}`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  deleteField: (id) => fetch(`${BASE}/fields/${id}`, { method: 'DELETE' }).then(r => r.json()),

  /* 扩展字段值 */
  updateTodoFields: (id, fields) => fetch(`${BASE}/todos/${id}/fields`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ fields }) }).then(r => r.json()),
}

export default api
