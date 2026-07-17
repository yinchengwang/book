/* ============================================================
 * GitHub Issue 待办工具 - Vue 3 前端逻辑
 * ============================================================ */

const { createApp, ref, computed, watch, onMounted } = Vue;

const API = {
  list:   (params) => fetch('/api/issues?' + new URLSearchParams(params)).then(r => r.json()),
  get:    (id)     => fetch('/api/issues/' + id).then(r => r.json()),
  create: (body)   => fetch('/api/issues', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  update: (id, b)  => fetch('/api/issues/' + id, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(b) }).then(r => r.json()),
  remove: (id)     => fetch('/api/issues/' + id, { method: 'DELETE' }).then(r => r.json()),
  addChecklist:    (id, text) => fetch('/api/issues/' + id + '/checklist', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ text }) }).then(r => r.json()),
  toggleChecklist: (id, itemId) => fetch('/api/issues/' + id + '/checklist/' + itemId, { method: 'PATCH' }).then(r => r.json()),
  removeChecklist: (id, itemId) => fetch('/api/issues/' + id + '/checklist/' + itemId, { method: 'DELETE' }).then(r => r.json()),
};

const PER_PAGE = 20;

createApp({
  setup() {
    const issues = ref([]);
    const total = ref(0);
    const page = ref(1);
    const totalPages = computed(() => Math.max(1, Math.ceil(total.value / PER_PAGE)));

    const search = ref('');
    const statusFilter = ref('all');

    const selectedId = ref(null);
    const current = ref(null);
    const checklist = ref([]);

    const loading = ref(false);
    const editing = ref(false);
    const editForm = ref({ title: '', description: '', labels: '' });

    const showCreate = ref(false);
    const createForm = ref({ title: '', description: '', labels: '' });

    const toast = ref(null);
    function showToast(msg, type = 'success') {
      toast.value = { msg, type };
      setTimeout(() => { toast.value = null; }, 2200);
    }

    /* ====== 加载列表 ====== */
    async function loadIssues() {
      loading.value = true;
      try {
        const r = await API.list({
          status: statusFilter.value,
          search: search.value,
          page: page.value,
          per_page: PER_PAGE,
        });
        if (r.code !== 0) throw new Error(r.msg || 'list failed');
        issues.value = r.data.items;
        total.value = r.data.total;
      } catch (e) {
        showToast('加载失败：' + e.message, 'error');
      } finally {
        loading.value = false;
      }
    }

    function changePage(p) {
      if (p < 1 || p > totalPages.value) return;
      page.value = p;
      loadIssues();
    }

    /* ====== 详情 ====== */
    async function selectIssue(id) {
      selectedId.value = id;
      current.value = null;
      checklist.value = [];
      editing.value = false;
      try {
        const r = await API.get(id);
        if (r.code !== 0) throw new Error(r.msg || 'get failed');
        current.value = r.data;
        checklist.value = r.data.checklist || [];
        editForm.value = {
          title: r.data.title,
          description: r.data.description || '',
          labels: parseLabels(r.data.labels).join(', '),
        };
      } catch (e) {
        showToast('加载详情失败：' + e.message, 'error');
        selectedId.value = null;
      }
    }

    async function toggleStatus() {
      if (!current.value) return;
      const newStatus = current.value.status === 'closed' ? 'open' : 'closed';
      const r = await API.update(current.value.id, { status: newStatus });
      if (r.code !== 0) return showToast(r.msg || '切换状态失败', 'error');
      current.value.status = newStatus;
      current.value.updated_at = r.data.updated_at;
      showToast(newStatus === 'closed' ? '已关闭' : '已重新打开');
      loadIssues();
    }

    async function deleteCurrent() {
      if (!current.value) return;
      if (!confirm(`确认删除 Issue #${current.value.id}？`)) return;
      const r = await API.remove(current.value.id);
      if (r.code !== 0) return showToast(r.msg || '删除失败', 'error');
      showToast('已删除');
      current.value = null;
      selectedId.value = null;
      loadIssues();
    }

    function startEdit() {
      if (!current.value) return;
      editForm.value = {
        title: current.value.title,
        description: current.value.description || '',
        labels: parseLabels(current.value.labels).join(', '),
      };
      editing.value = true;
    }

    function cancelEdit() {
      editing.value = false;
    }

    async function saveEdit() {
      if (!editForm.value.title) return;
      const r = await API.update(current.value.id, {
        title: editForm.value.title,
        description: editForm.value.description,
        labels: parseLabels(editForm.value.labels),
      });
      if (r.code !== 0) return showToast(r.msg || '保存失败', 'error');
      current.value = r.data;
      editing.value = false;
      showToast('已保存');
      loadIssues();
    }

    /* ====== Checklist ====== */
    const newChecklistText = ref('');

    const doneCount = computed(() => checklist.value.filter(i => i.done).length);
    const progressPct = computed(() => checklist.value.length ? Math.round(doneCount.value * 100 / checklist.value.length) : 0);

    async function addChecklistItem() {
      if (!newChecklistText.value || !current.value) return;
      const r = await API.addChecklist(current.value.id, newChecklistText.value);
      if (r.code !== 0) return showToast(r.msg || '添加失败', 'error');
      checklist.value.push(r.data);
      newChecklistText.value = '';
    }

    async function toggleChecklistItem(itemId) {
      const item = checklist.value.find(i => i.id === itemId);
      if (!item) return;
      const r = await API.toggleChecklist(current.value.id, itemId);
      if (r.code !== 0) return showToast(r.msg || '切换失败', 'error');
      item.done = item.done ? 0 : 1;
    }

    async function removeChecklistItem(itemId) {
      if (!confirm('确认删除这个待办项？')) return;
      const r = await API.removeChecklist(current.value.id, itemId);
      if (r.code !== 0) return showToast(r.msg || '删除失败', 'error');
      checklist.value = checklist.value.filter(i => i.id !== itemId);
    }

    /* ====== 新建 ====== */
    function openCreate() {
      createForm.value = { title: '', description: '', labels: '' };
      showCreate.value = true;
    }

    async function submitCreate() {
      if (!createForm.value.title) return;
      const r = await API.create({
        title: createForm.value.title,
        description: createForm.value.description,
        labels: parseLabels(createForm.value.labels),
      });
      if (r.code !== 0) return showToast(r.msg || '创建失败', 'error');
      showCreate.value = false;
      showToast('已创建');
      await loadIssues();
      selectIssue(r.data.id);
    }

    /* ====== 工具 ====== */
    function parseLabels(s) {
      if (Array.isArray(s)) return s;
      if (!s) return [];
      try {
        const arr = JSON.parse(s);
        if (Array.isArray(arr)) return arr;
      } catch {}
      return [];
    }

    function formatTime(ts) {
      if (!ts) return '';
      const d = new Date(ts * 1000);
      const now = new Date();
      const sameDay = d.toDateString() === now.toDateString();
      if (sameDay) {
        return d.getHours().toString().padStart(2, '0') + ':' +
               d.getMinutes().toString().padStart(2, '0');
      }
      return d.getFullYear() + '-' +
             (d.getMonth() + 1).toString().padStart(2, '0') + '-' +
             d.getDate().toString().padStart(2, '0');
    }

    // 状态变更、搜索后重新加载（防抖简单）
    let searchTimer = null;
    watch(search, () => {
      clearTimeout(searchTimer);
      searchTimer = setTimeout(() => { page.value = 1; loadIssues(); }, 300);
    });
    watch(statusFilter, () => { page.value = 1; loadIssues(); });

    onMounted(loadIssues);

    return {
      issues, total, page, totalPages, search, statusFilter,
      selectedId, current, checklist,
      loading, editing, editForm,
      showCreate, createForm,
      toast, newChecklistText,
      doneCount, progressPct,
      loadIssues, changePage,
      selectIssue, toggleStatus, deleteCurrent,
      startEdit, cancelEdit, saveEdit,
      addChecklistItem, toggleChecklistItem, removeChecklistItem,
      openCreate, submitCreate,
      parseLabels, formatTime,
    };
  }
}).mount('#app');
