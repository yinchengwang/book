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
}

export default api
