#!/usr/bin/env node
/**
 * 读书雷达本地服务器
 *
 * 双重角色：
 *   1. 静态文件服务 — 提供 HTML/JS/CSS/数据文件
 *   2. REST API — 用户数据 CRUD（存在 user-data/ 目录，Git 追踪）
 *
 * 用法：
 *   node server.js [端口]
 *
 * 零外部依赖 — 仅使用 Node.js 内置模块。
 */

'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

// ============================================================
// 配置
// ============================================================
const PORT = parseInt(process.argv[2], 10) || 8080;
const ROOT = __dirname;
const DATA_DIR = path.join(ROOT, 'user-data');
const DIRS = ['state', 'books', 'projects'];

// 确保数据目录存在
for (const sub of DIRS) {
  const p = path.join(DATA_DIR, sub);
  if (!fs.existsSync(p)) fs.mkdirSync(p, { recursive: true });
}

// ============================================================
// MIME 类型
// ============================================================
const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.pdf': 'application/pdf',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.txt': 'text/plain; charset=utf-8',
  '.md': 'text/markdown; charset=utf-8',
  '.xlsx': 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
  '.py': 'text/plain; charset=utf-8',
  '.zip': 'application/zip',
};

// ============================================================
// 辅助函数
// ============================================================
function log(msg) {
  const ts = new Date().toLocaleTimeString('zh-CN', { hour12: false });
  process.stdout.write(`[${ts}] ${msg}\n`);
}

function json(res, code, data) {
  res.writeHead(code, {
    'Content-Type': 'application/json; charset=utf-8',
    'Access-Control-Allow-Origin': '*',
  });
  res.end(JSON.stringify(data));
}

function error(res, code, message) {
  json(res, code, { error: message });
}

// 获取本机局域网 IP
function getLocalIPs() {
  const os = require('os');
  const ifaces = os.networkInterfaces();
  const ips = [];
  for (const name of Object.keys(ifaces)) {
    for (const iface of ifaces[name]) {
      if (iface.family === 'IPv4' && !iface.internal && !iface.address.startsWith('169.254')) {
        ips.push(iface.address);
      }
    }
  }
  return ips;
}

// 从 URL 路径提取路由参数
// /api/state/:app  → { app }
// /api/books/:mode/:bookId/pdf → { mode, bookId }
// /api/projects/:catId/:projId/files → { catId, projId }
// /api/projects/:catId/:projId/files/:filename → { catId, projId, filename }
function matchRoute(pathname) {
  const parts = pathname.split('/').filter(Boolean);
  if (parts[0] !== 'api') return null;

  // /api/state/:app
  if (parts.length === 3 && parts[1] === 'state') {
    return { type: 'state', app: parts[2] };
  }

  // /api/books/:mode/:bookId/pdf
  if (parts.length === 5 && parts[1] === 'books' && parts[4] === 'pdf') {
    return { type: 'book-pdf', mode: parts[2], bookId: parts[3] };
  }

  // /api/projects/:catId/:projId/files
  if (parts.length === 5 && parts[1] === 'projects' && parts[4] === 'files') {
    return { type: 'project-files', catId: parts[2], projId: parts[3] };
  }

  // /api/projects/:catId/:projId/files/:filename
  if (parts.length === 6 && parts[1] === 'projects' && parts[4] === 'files') {
    return { type: 'project-file', catId: parts[2], projId: parts[3], filename: parts[5] };
  }

  // /api/export
  if (parts.length === 2 && parts[1] === 'export') {
    return { type: 'export' };
  }

  // /api/excerpts/* — 技术摘抄
  if (parts[1] === 'excerpts') {
    // /api/excerpts/books
    if (parts.length === 3 && parts[2] === 'books') return { type: 'excerpts-books' };
    // /api/excerpts/suggestions
    if (parts.length === 3 && parts[2] === 'suggestions') return { type: 'excerpts-suggestions' };
    // /api/excerpts/export
    if (parts.length === 3 && parts[2] === 'export') return { type: 'excerpts-export' };
    // /api/excerpts/:id/meta
    if (parts.length === 4 && parts[3] === 'meta') return { type: 'excerpts-meta', id: parts[2] };
    // /api/excerpts/:id/note
    if (parts.length === 4 && parts[3] === 'note') return { type: 'excerpts-note', id: parts[2] };
    // /api/excerpts/:id/notes
    if (parts.length === 4 && parts[3] === 'notes') return { type: 'excerpts-notes', id: parts[2] };
    // /api/excerpts/:id/favorite
    if (parts.length === 4 && parts[3] === 'favorite') return { type: 'excerpts-favorite', id: parts[2] };
    // /api/excerpts/books/:id/status — 更新书籍状态
    if (parts.length === 5 && parts[2] === 'books' && parts[4] === 'status') return { type: 'excerpts-book-status', id: parts[3] };
    // /api/excerpts/:id — 删除摘抄 / 更新摘抄元数据（通过 method 区分）
    if (parts.length === 3 && parts[2] !== 'books' && parts[2] !== 'suggestions' && parts[2] !== 'export') return { type: 'excerpts-item', id: parts[2] };
    // /api/excerpts (列表/搜索)
    if (parts.length === 2) return { type: 'excerpts-list' };
    return { type: 'excerpts-deep', path: parts.slice(1).join('/') };
  }

  // /api/tracker — 面试追踪
  if (parts[1] === 'tracker') {
    if (parts.length === 2) return { type: 'tracker-root' };
    // /api/tracker/:companyId (DELETE company 或 GET meta)
    if (parts.length === 3) return { type: 'tracker-company', companyId: parts[2] };
    // /api/tracker/:companyId/meta (GET/PUT meta)
    if (parts.length === 4 && parts[3] === 'meta') return { type: 'tracker-company-meta', companyId: parts[2] };
    // /api/tracker/:companyId/interviews (GET rounds)
    if (parts.length === 4 && parts[3] === 'interviews') return { type: 'tracker-rounds', companyId: parts[2] };
    // /api/tracker/:companyId/:resource (GET/PUT jd.md, notes.md, etc.)
    if (parts.length === 4) return { type: 'tracker-file', companyId: parts[2], resource: parts[3] };
    // /api/tracker/:companyId/interviews/:roundId (PUT round)
    if (parts.length === 5 && parts[3] === 'interviews') return { type: 'tracker-round-update', companyId: parts[2], roundId: parts[4] };
    return { type: 'tracker-deep', companyId: parts.slice(1).join('/') };
  }

  return null;
}

// ============================================================
// 静态文件服务
// ============================================================
function serveStatic(req, res, pathname) {
  // 规范化路径防止目录遍历
  let safePath = path.normalize(pathname).replace(/^[/\\]+/, '');

  // 根路径 → 列出目录
  if (!safePath || safePath === '.') {
    return serveDirectory(res, ROOT);
  }

  const filePath = path.join(ROOT, safePath);

  // 安全检查：确保在 ROOT 内
  if (!filePath.startsWith(ROOT)) {
    error(res, 403, '禁止访问');
    return;
  }

  // 请求目录 → 显示目录列表
  if (fs.existsSync(filePath) && fs.statSync(filePath).isDirectory()) {
    return serveDirectory(res, filePath, pathname);
  }

  // 请求文件 → 返回文件内容
  const ext = path.extname(filePath).toLowerCase();
  const mime = MIME[ext] || 'application/octet-stream';

  try {
    const data = fs.readFileSync(filePath);
    res.writeHead(200, {
      'Content-Type': mime,
      'Content-Length': data.length,
      'Access-Control-Allow-Origin': '*',
    });
    res.end(data);
  } catch (e) {
    if (e.code === 'ENOENT') {
      // 检查是否是 data/ 下文件的直接请求，返回 404
      error(res, 404, '文件未找到');
    } else {
      error(res, 500, '读取文件失败');
    }
    log(`静态文件错误: ${safePath} — ${e.message}`);
  }
}

function serveDirectory(res, dirPath, urlPath) {
  const entries = fs.readdirSync(dirPath, { withFileTypes: true });
  const parentDir = urlPath ? path.dirname(urlPath) : '/';
  const items = [];

  for (const e of entries) {
    // 跳过隐藏文件和 node_modules
    if (e.name.startsWith('.') || e.name === 'node_modules') continue;
    const full = path.join(dirPath, e.name);
    const stat = fs.statSync(full);
    items.push({
      name: e.name,
      type: e.isDirectory() ? 'dir' : 'file',
      size: stat.size,
      mtime: stat.mtime.toISOString(),
    });
  }

  items.sort((a, b) => {
    if (a.type !== b.type) return a.type === 'dir' ? -1 : 1;
    return a.name.localeCompare(b.name);
  });

  const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>读书雷达 — 文件目录</title>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"PingFang SC","Microsoft YaHei",sans-serif;max-width:800px;margin:40px auto;padding:0 20px;background:#f8f9fa;color:#2c3e50}
h1{font-size:20px;margin-bottom:8px}
.sub{color:#7f8c8d;font-size:13px;margin-bottom:24px}
table{width:100%;border-collapse:collapse;background:#fff;border-radius:8px;box-shadow:0 1px 4px rgba(0,0,0,0.06)}
td{padding:10px 16px;border-bottom:1px solid #f0f0f0;font-size:14px}
tr:last-child td{border-bottom:none}
a{color:#2980b9;text-decoration:none}
a:hover{text-decoration:underline}
a.dir{font-weight:600}
.size{color:#7f8c8d;text-align:right;white-space:nowrap}
.date{color:#95a5a6;text-align:right;font-size:12px}
.footer{text-align:center;margin-top:32px;font-size:12px;color:#95a5a6}
</style>
</head>
<body>
<h1>📡 读书雷达</h1>
<div class="sub">${urlPath || '/'} 目录 — ${items.length} 个项目</div>
<table>
<tr><td colspan="3"><a href="${parentDir}" class="dir">📁 ../</a></td></tr>
${items.map(i => `<tr>
<td>${i.type === 'dir' ? '📁' : '📄'} <a href="${urlPath || ''}/${i.name}"${i.type === 'dir' ? ' class="dir"' : ''}>${i.name}</a></td>
<td class="date">${formatDate(i.mtime)}</td>
<td class="size">${i.type === 'dir' ? '—' : formatSize(i.size)}</td>
</tr>`).join('\n')}
</table>
<div class="footer">可直接打开 *.html 文件使用，或通过本服务访问 API</div>
</body>
</html>`;

  res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
  res.end(html);
}

function formatSize(bytes) {
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
  return (bytes / 1048576).toFixed(1) + ' MB';
}

function formatDate(iso) {
  const d = new Date(iso);
  const pad = n => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

// ============================================================
// 状态文件映射
// ============================================================
const STATE_FILES = {
  index: 'index-state.json',
  kanban: 'kanban-state.json',
  quiz: 'quiz-state.json',
  'five-year-plan': 'five-year-plan-state.json',
};

function getStateFile(app) {
  const filename = STATE_FILES[app];
  if (!filename) return null;
  return path.join(DATA_DIR, 'state', filename);
}

// ============================================================
// API: 状态读写
// ============================================================

// GET /api/state/:app
function handleGetState(res, app) {
  const file = getStateFile(app);
  if (!file) { error(res, 400, '无效的应用名称'); return; }

  try {
    if (fs.existsSync(file)) {
      const raw = fs.readFileSync(file, 'utf8');
      const data = JSON.parse(raw);
      json(res, 200, data);
    } else {
      // 返回空结构
      json(res, 200, getDefaultState(app));
    }
  } catch (e) {
    log(`读取状态失败: ${file} — ${e.message}`);
    error(res, 500, '读取状态失败');
  }
}

// PUT /api/state/:app
function handlePutState(res, app, body) {
  const file = getStateFile(app);
  if (!file) { error(res, 400, '无效的应用名称'); return; }

  try {
    // 验证 JSON 可解析
    const data = JSON.parse(body);

    // 确保目录存在
    const dir = path.dirname(file);
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });

    // 原子写入：先写临时文件，再重命名
    const tmp = file + '.tmp';
    fs.writeFileSync(tmp, JSON.stringify(data, null, 2), 'utf8');
    fs.renameSync(tmp, file);

    log(`状态已保存: ${app} (${body.length} bytes)`);
    json(res, 200, { ok: true });
  } catch (e) {
    if (e instanceof SyntaxError) {
      error(res, 400, '无效的 JSON 格式');
    } else {
      log(`保存状态失败: ${file} — ${e.message}`);
      error(res, 500, '保存状态失败');
    }
  }
}

// 默认状态结构
function getDefaultState(app) {
  switch (app) {
    case 'index': return { books: {} };
    case 'kanban': return { kanban: {} };
    case 'quiz': return { history: {}, projects: {}, checklist: {}, dailyTip: null };
    case 'five-year-plan': return {};
    default: return {};
  }
}

// ============================================================
// API: PDF 文件操作
// ============================================================

function getPDFPath(mode, bookId) {
  // 防止路径遍历
  const safeMode = mode.replace(/[<>:"/\\|?*]/g, '_');
  const safeBookId = bookId.replace(/[<>:"/\\|?*]/g, '_');
  const dir = path.join(DATA_DIR, 'books', safeMode);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  return path.join(dir, safeBookId + '.pdf');
}

// GET /api/books/:mode/:bookId/pdf
function handleGetPDF(res, mode, bookId) {
  const pdfPath = getPDFPath(mode, bookId);
  try {
    if (!fs.existsSync(pdfPath)) {
      error(res, 404, 'PDF 未找到');
      return;
    }
    const stat = fs.statSync(pdfPath);
    const data = fs.readFileSync(pdfPath);
    res.writeHead(200, {
      'Content-Type': 'application/pdf',
      'Content-Length': stat.size,
      'Access-Control-Allow-Origin': '*',
    });
    res.end(data);
  } catch (e) {
    log(`读取 PDF 失败: ${pdfPath} — ${e.message}`);
    error(res, 500, '读取 PDF 失败');
  }
}

// POST /api/books/:mode/:bookId/pdf — multipart 上传
function handlePostPDF(res, mode, bookId, req) {
  parseMultipart(req, (err, parts) => {
    if (err) { error(res, 400, '解析上传数据失败: ' + err); return; }

    const filePart = parts.find(p => p.filename);
    if (!filePart) { error(res, 400, '未找到上传文件'); return; }

    const pdfPath = getPDFPath(mode, bookId);
    try {
      fs.writeFileSync(pdfPath, filePart.data);
      log(`PDF 已保存: ${mode}/${bookId}.pdf (${filePart.data.length} bytes)`);
      json(res, 200, { ok: true, filename: filePart.filename, size: filePart.data.length });
    } catch (e) {
      log(`保存 PDF 失败: ${pdfPath} — ${e.message}`);
      error(res, 500, '保存 PDF 失败');
    }
  });
}

// DELETE /api/books/:mode/:bookId/pdf
function handleDeletePDF(res, mode, bookId) {
  const pdfPath = getPDFPath(mode, bookId);
  try {
    if (!fs.existsSync(pdfPath)) {
      error(res, 404, 'PDF 未找到');
      return;
    }
    fs.unlinkSync(pdfPath);
    log(`PDF 已删除: ${mode}/${bookId}.pdf`);
    json(res, 200, { ok: true });
  } catch (e) {
    log(`删除 PDF 失败: ${pdfPath} — ${e.message}`);
    error(res, 500, '删除 PDF 失败');
  }
}

// ============================================================
// API: 项目文件操作
// ============================================================

function getProjectDir(catId, projId) {
  const safeCat = catId.replace(/[<>:"/\\|?*]/g, '_');
  const safeProj = projId.replace(/[<>:"/\\|?*]/g, '_');
  const dir = path.join(DATA_DIR, 'projects', `${safeCat}_${safeProj}`);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  return dir;
}

// POST /api/projects/:catId/:projId/files — 上传项目文件
function handlePostProjectFiles(res, catId, projId, req) {
  parseMultipart(req, (err, parts) => {
    if (err) { error(res, 400, '解析上传数据失败: ' + err); return; }

    const saved = [];
    const dir = getProjectDir(catId, projId);

    for (const part of parts) {
      if (!part.filename) continue;
      const safeName = part.filename.replace(/[<>:"/\\|?*]/g, '_');
      const filePath = path.join(dir, safeName);
      fs.writeFileSync(filePath, part.data);
      saved.push({ filename: part.filename, size: part.data.length });
    }

    log(`项目文件已保存: ${catId}/${projId} — ${saved.map(p => p.filename).join(', ')}`);
    json(res, 200, { ok: true, files: saved });
  });
}

// GET /api/projects/:catId/:projId/files/:filename
function handleGetProjectFile(res, catId, projId, filename) {
  const safeName = filename.replace(/[<>:"/\\|?*]/g, '_');
  const filePath = path.join(getProjectDir(catId, projId), safeName);
  try {
    if (!fs.existsSync(filePath)) {
      error(res, 404, '文件未找到');
      return;
    }
    const stat = fs.statSync(filePath);
    const data = fs.readFileSync(filePath);
    const ext = path.extname(filePath).toLowerCase();
    const mime = MIME[ext] || 'text/plain; charset=utf-8';
    res.writeHead(200, {
      'Content-Type': mime,
      'Content-Length': stat.size,
      'Access-Control-Allow-Origin': '*',
    });
    res.end(data);
  } catch (e) {
    log(`读取项目文件失败: ${filePath} — ${e.message}`);
    error(res, 500, '读取文件失败');
  }
}

// DELETE /api/projects/:catId/:projId/files/:filename
function handleDeleteProjectFile(res, catId, projId, filename) {
  const safeName = filename.replace(/[<>:"/\\|?*]/g, '_');
  const filePath = path.join(getProjectDir(catId, projId), safeName);
  try {
    if (!fs.existsSync(filePath)) {
      error(res, 404, '文件未找到');
      return;
    }
    fs.unlinkSync(filePath);
    log(`项目文件已删除: ${catId}/${projId}/${filename}`);
    json(res, 200, { ok: true });
  } catch (e) {
    log(`删除项目文件失败: ${filePath} — ${e.message}`);
    error(res, 500, '删除文件失败');
  }
}

// ============================================================
// Multipart 解析器（手写，零外部依赖）
// ============================================================
function parseMultipart(req, callback) {
  const contentType = req.headers['content-type'] || '';
  const match = contentType.match(/boundary=(.+)$/);
  if (!match) { callback('未找到 boundary', null); return; }

  const boundary = match[1].trim();
  // boundary 可能带引号
  const cleanBoundary = boundary.replace(/^["'](.*)["']$/, '$1');

  const chunks = [];
  req.on('data', chunk => chunks.push(chunk));
  req.on('end', () => {
    try {
      const buffer = Buffer.concat(chunks);
      const parts = parseMultipartBody(buffer, cleanBoundary);
      callback(null, parts);
    } catch (e) {
      callback(e.message, null);
    }
  });
  req.on('error', () => callback('请求读取失败', null));
}

function parseMultipartBody(buffer, boundary) {
  const parts = [];
  const boundaryBuf = Buffer.from('--' + boundary);
  const endBoundaryBuf = Buffer.from('--' + boundary + '--');
  const crlf = Buffer.from('\r\n');
  const doubleCrlf = Buffer.from('\r\n\r\n');

  // 找到所有 boundary 位置
  let pos = 0;
  let contents = [];

  while (pos < buffer.length) {
    const idx = buffer.indexOf(boundaryBuf, pos);
    if (idx === -1) break;

    const contentStart = idx + boundaryBuf.length;
    // 跳过可能的后缀 '--'（结束标记）
    if (buffer.slice(contentStart, contentStart + 2).toString() === '--') break;

    // 跳过 CRLF
    const afterBoundary = contentStart + (buffer[contentStart] === 13 ? 2 : 0);

    // 找下一个 boundary
    const nextIdx = buffer.indexOf(boundaryBuf, afterBoundary);
    if (nextIdx === -1) {
      // 最后一个，到 buffer 结尾
      contents.push(buffer.slice(afterBoundary));
      break;
    }
    // 内容到下一个 boundary 之前（去掉尾部 CRLF）
    let contentEnd = nextIdx;
    while (contentEnd > afterBoundary && buffer[contentEnd - 1] === 10) contentEnd--;
    if (contentEnd > afterBoundary && buffer[contentEnd - 1] === 13) contentEnd--;
    contents.push(buffer.slice(afterBoundary, contentEnd));
    pos = nextIdx;
  }

  for (const content of contents) {
    // 分离头部和主体
    const headerEnd = content.indexOf(doubleCrlf);
    if (headerEnd === -1) continue;

    const headerStr = content.slice(0, headerEnd).toString('utf8');
    const body = content.slice(headerEnd + 4);

    // 解析 Content-Disposition
    const nameMatch = headerStr.match(/name="([^"]+)"/);
    const filenameMatch = headerStr.match(/filename="([^"]+)"/);

    if (nameMatch) {
      parts.push({
        name: nameMatch[1],
        filename: filenameMatch ? filenameMatch[1] : null,
        data: body,
      });
    }
  }

  return parts;
}

// ============================================================
// CORS 预检
// ============================================================
function handleCORS(res) {
  res.writeHead(204, {
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, PUT, POST, DELETE, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type',
    'Access-Control-Max-Age': '86400',
  });
  res.end();
}

// ============================================================
// 面试追踪 API
// ============================================================
const TRACKER_DIR = path.join(ROOT, 'data', 'interview-tracker');

function ensureTrackerDir() {
  if (!fs.existsSync(TRACKER_DIR)) fs.mkdirSync(TRACKER_DIR, { recursive: true });
}

function sanitizeId(id) {
  return String(id).replace(/[<>:"/\\|?*]/g, '_');
}

function getTrackerDir(companyId) {
  ensureTrackerDir();
  var safeId = sanitizeId(companyId);
  var dir = path.join(TRACKER_DIR, safeId);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  return { dir: dir, safeId: safeId };
}

function handleTrackerAPI(req, res, route, method, body) {
  var type = route.type;

  // GET /api/tracker — 返回索引
  if (type === 'tracker-root' && method === 'GET') {
    var idxPath = path.join(TRACKER_DIR, '_index.json');
    try {
      if (fs.existsSync(idxPath)) {
        json(res, 200, JSON.parse(fs.readFileSync(idxPath, 'utf8')));
      } else {
        json(res, 200, { version: 1, lastModified: '', companies: [] });
      }
    } catch (e) { error(res, 500, '读取索引失败'); }
    return;
  }

  // PUT /api/tracker — 更新索引
  if (type === 'tracker-root' && method === 'PUT') {
    try {
      fs.writeFileSync(path.join(TRACKER_DIR, '_index.json'), body, 'utf8');
      json(res, 200, { ok: true });
    } catch (e) { error(res, 400, '保存失败'); }
    return;
  }

  // POST /api/tracker — 创建公司
  if (type === 'tracker-root' && method === 'POST') {
    try {
      var company = JSON.parse(body);
      var _r = getTrackerDir(company.companyId || ('c-' + Date.now()));
      var safeId = _r.safeId;
      var companyDir = _r.dir;
      var fm = '---\n';
      for (var k in company) {
        if (k === 'companyId') continue;
        if (typeof company[k] === 'object') fm += k + ': ' + JSON.stringify(company[k]) + '\n';
        else fm += k + ': ' + company[k] + '\n';
      }
      fm += '---\n';
      fs.writeFileSync(path.join(companyDir, '_meta.md'), fm, 'utf8');
      fs.writeFileSync(path.join(companyDir, 'jd.md'), '', 'utf8');
      fs.writeFileSync(path.join(companyDir, 'comprehensive.md'), '', 'utf8');
      fs.writeFileSync(path.join(companyDir, 'notes.md'), '', 'utf8');
      var idxPath = path.join(TRACKER_DIR, '_index.json');
      var idx = { version: 1, lastModified: new Date().toISOString(), companies: [] };
      if (fs.existsSync(idxPath)) idx = JSON.parse(fs.readFileSync(idxPath, 'utf8'));
      idx.companies.push(company);
      fs.writeFileSync(idxPath, JSON.stringify(idx, null, 2), 'utf8');
      json(res, 200, { ok: true, companyId: safeId });
    } catch (e) { error(res, 500, '创建失败: ' + e.message); }
    return;
  }

  // GET /api/tracker/:companyId/meta
  if (type === 'tracker-company-meta' && method === 'GET') {
    var idxPath = path.join(TRACKER_DIR, '_index.json');
    try {
      if (!fs.existsSync(idxPath)) return error(res, 404, '索引不存在');
      var idx = JSON.parse(fs.readFileSync(idxPath, 'utf8'));
      for (var i = 0; i < idx.companies.length; i++) {
        if (idx.companies[i].companyId === route.companyId) { json(res, 200, idx.companies[i]); return; }
      }
      error(res, 404, '公司不存在');
    } catch (e) { error(res, 500, '读取失败'); }
    return;
  }

  // PUT /api/tracker/:companyId/meta
  if (type === 'tracker-company-meta' && method === 'PUT') {
    try {
      var updates = JSON.parse(body);
      var idxPath = path.join(TRACKER_DIR, '_index.json');
      var idx = fs.existsSync(idxPath) ? JSON.parse(fs.readFileSync(idxPath, 'utf8')) : { version: 1, companies: [] };
      var _r2 = getTrackerDir(route.companyId);
      for (var i = 0; i < idx.companies.length; i++) {
        if (idx.companies[i].companyId === route.companyId) {
          for (var k in updates) idx.companies[i][k] = updates[k];
          break;
        }
      }
      fs.writeFileSync(idxPath, JSON.stringify(idx, null, 2), 'utf8');
      var fm2 = '---\n';
      for (var k in idx.companies[i]) {
        if (k === 'companyId') continue;
        if (typeof idx.companies[i][k] === 'object') fm2 += k + ': ' + JSON.stringify(idx.companies[i][k]) + '\n';
        else fm2 += k + ': ' + idx.companies[i][k] + '\n';
      }
      fm2 += '---\n';
      fs.writeFileSync(path.join(_r2.dir, '_meta.md'), fm2, 'utf8');
      json(res, 200, { ok: true });
    } catch (e) { error(res, 400, '更新失败: ' + e.message); }
    return;
  }

  // DELETE /api/tracker/:companyId
  if (type === 'tracker-company' && method === 'DELETE') {
    try {
      var safeId = sanitizeId(route.companyId);
      var companyDir = path.join(TRACKER_DIR, safeId);
      if (fs.existsSync(companyDir)) {
        var rmDir = function(p) {
          var entries = fs.readdirSync(p);
          for (var i = 0; i < entries.length; i++) {
            var fullPath = path.join(p, entries[i]);
            if (fs.statSync(fullPath).isDirectory()) rmDir(fullPath);
            else fs.unlinkSync(fullPath);
          }
          fs.rmdirSync(p);
        };
        rmDir(companyDir);
      }
      var idxPath = path.join(TRACKER_DIR, '_index.json');
      var idx = fs.existsSync(idxPath) ? JSON.parse(fs.readFileSync(idxPath, 'utf8')) : { version: 1, companies: [] };
      idx.companies = idx.companies.filter(function(c){ return c.companyId !== route.companyId; });
      fs.writeFileSync(idxPath, JSON.stringify(idx, null, 2), 'utf8');
      json(res, 200, { ok: true });
    } catch (e) { error(res, 500, '删除失败: ' + e.message); }
    return;
  }

  // GET /api/tracker/:companyId/:resource
  if (type === 'tracker-file' && method === 'GET') {
    try {
      var safeId = sanitizeId(route.companyId);
      var compDir = path.join(TRACKER_DIR, safeId);
      var mdPath = path.join(compDir, route.resource);
      // 去掉末尾的 .md
      var baseResource = mdPath.replace(/\.md$/, '');
      if (!fs.existsSync(mdPath)) {
        // 可能是目录（如 interviews）
        if (fs.existsSync(baseResource) && fs.statSync(baseResource).isDirectory()) {
          var files = fs.readdirSync(baseResource).filter(function(f){ return /^round-\d+\.md$/.test(f); }).sort();
          var rounds = [];
          for (var i = 0; i < files.length; i++) {
            var raw = fs.readFileSync(path.join(baseResource, files[i]), 'utf8');
            var content = raw.replace(/^---\n[\s\S]*?\n---\n?/, '');
            var m = files[i].match(/round-(\d+)/);
            rounds.push({ id: m ? m[1] : i, content: content, file: files[i] });
          }
          json(res, 200, rounds);
          return;
        }
        return error(res, 404, '文件不存在');
      }
      var content = fs.readFileSync(mdPath, 'utf8');
      content = content.replace(/^---\n[\s\S]*?\n---\n?/, '');
      json(res, 200, { content: content });
    } catch (e) { error(res, 500, '读取失败: ' + e.message); }
    return;
  }

  // GET /api/tracker/:companyId/interviews
  if (type === 'tracker-rounds' && method === 'GET') {
    try {
      var safeId = sanitizeId(route.companyId);
      var intDir = path.join(TRACKER_DIR, safeId, 'interviews');
      var rounds = [];
      if (fs.existsSync(intDir)) {
        var files = fs.readdirSync(intDir).filter(function(f){ return /^round-\d+\.md$/.test(f); }).sort();
        for (var i = 0; i < files.length; i++) {
          var raw = fs.readFileSync(path.join(intDir, files[i]), 'utf8');
          var content = raw.replace(/^---\n[\s\S]*?\n---\n?/, '');
          var m = files[i].match(/round-(\d+)/);
          rounds.push({ id: m ? m[1] : i, content: content, file: files[i] });
        }
      }
      json(res, 200, rounds);
    } catch (e) { error(res, 500, '读取失败: ' + e.message); }
    return;
  }

  // PUT /api/tracker/:companyId/:resource
  if (type === 'tracker-file' && method === 'PUT') {
    try {
      var data = JSON.parse(body);
      var safeId = sanitizeId(route.companyId);
      var compDir = path.join(TRACKER_DIR, safeId);
      var mdPath = path.join(compDir, route.resource);
      var dir = path.dirname(mdPath);
      if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
      fs.writeFileSync(mdPath, data.content || '', 'utf8');
      json(res, 200, { ok: true });
    } catch (e) { error(res, 500, '保存失败: ' + e.message); }
    return;
  }

  // PUT /api/tracker/:companyId/interviews/:roundId
  if (type === 'tracker-round-update' && method === 'PUT') {
    try {
      var data = JSON.parse(body);
      var safeId = sanitizeId(route.companyId);
      var intDir = path.join(TRACKER_DIR, safeId, 'interviews');
      if (!fs.existsSync(intDir)) fs.mkdirSync(intDir, { recursive: true });
      fs.writeFileSync(path.join(intDir, 'round-' + route.roundId + '.md'), data.content || '', 'utf8');
      json(res, 200, { ok: true });
    } catch (e) { error(res, 500, '保存失败: ' + e.message); }
    return;
  }

  // PUT /api/tracker/:companyId/interviews
  if (type === 'tracker-rounds' && method === 'PUT') {
    try {
      var data = JSON.parse(body);
      var safeId = sanitizeId(route.companyId);
      var intDir = path.join(TRACKER_DIR, safeId, 'interviews');
      if (!fs.existsSync(intDir)) fs.mkdirSync(intDir, { recursive: true });
      var roundFile = 'round-' + (data.id || '1') + '.md';
      fs.writeFileSync(path.join(intDir, roundFile), data.content || '', 'utf8');
      json(res, 200, { ok: true });
    } catch (e) { error(res, 500, '保存失败: ' + e.message); }
    return;
  }

  error(res, 404, '未知的 tracker 端点');
}

// ============================================================
// 技术摘抄 API
// ============================================================
const EXCERPT_DIR = path.join(ROOT, 'data', 'excerpt');

// 缓存
let _excerptsCache = null;
let _excerptsCacheTime = 0;
const CACHE_TTL = 60000; // 60 秒

function sanitizeId(id) {
  return String(id).replace(/[<>:"/\\|?*]/g, '_');
}

/**
 * 转义正则表达式特殊字符
 */
function escapeRegex(str) {
  return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

/**
 * 扫描所有 .md 文件，构建索引
 */
function scanExcerpts() {
  const now = Date.now();
  if (_excerptsCache && (now - _excerptsCacheTime) < CACHE_TTL) {
    return _excerptsCache;
  }

  const books = {};       // bookId -> { id, title, count, tags, year, noteCount, lastUpdated, status }
  const tagCount = {};    // tagName -> count
  const yearSet = new Set();
  let totalExcerpts = 0;

  function walkDir(dir, relPrefix) {
    if (!fs.existsSync(dir)) return;
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(dir, entry.name);
      const rel = relPrefix ? `${relPrefix}/${entry.name}` : entry.name;

      if (entry.isDirectory()) {
        // 检查是否是年份目录
        const isYear = /^\d{4}$/.test(entry.name);
        walkDir(fullPath, isYear ? entry.name : rel);
      } else if (entry.isFile() && entry.name.endsWith('.md')) {
        try {
          const raw = fs.readFileSync(fullPath, 'utf8');
          // 解析 frontmatter
          const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
          let book = '';
          let author = '';
          let date = '';
          let tags = [];
          let body = '';

          if (fmMatch) {
            const fmText = fmMatch[1];
            const bookMatch = fmText.match(/book:\s*"([^"]+)"/);
            const authorMatch = fmText.match(/author:\s*"([^"]+)"/);
            const dateMatch = fmText.match(/date:\s*"([^"]+)"/);
            const tagsMatch = fmText.match(/tags:\s*\[([^\]]+)\]/);

            if (bookMatch) book = bookMatch[1];
            if (authorMatch) author = authorMatch[1];
            if (dateMatch) date = dateMatch[1];
            if (tagsMatch) tags = tagsMatch[1].split(',').map(t => t.trim()).filter(t => t);
            body = raw.substring(fmMatch[0].length);
          } else {
            // 无 frontmatter，从文件名推断
            const baseName = path.basename(entry.name, '.md');
            book = baseName.split('_')[0];
            body = raw;
          }

          if (!book) book = path.basename(entry.name, '.md');

          // 推断年份
          let year = null;
          const segments = rel.split(/[/\\]/);
          for (const seg of segments) {
            if (/^\d{4}$/.test(seg)) { year = seg; break; }
          }

          // 构建 bookId
          const bookId = sanitizeId(book).toLowerCase().replace(/\s+/g, '-');

          // 统计
          if (!books[bookId]) {
            books[bookId] = {
              id: bookId,
              title: book,
              count: 0,           // 摘抄段落数（## 摘抄的数量）
              tags: [],
              year: year,
              noteCount: 0,
              lastUpdated: null,  // 最后更新时间（最新摘抄日期）
              status: 'unread',   // 书籍状态：unread/reading/done/want/abandoned
            };
          }
          books[bookId].count++;
          totalExcerpts++;

          // 记录最后更新时间
          if (date) {
            if (!books[bookId].lastUpdated || date > books[bookId].lastUpdated) {
              books[bookId].lastUpdated = date;
            }
          }

          tags.forEach(t => {
            tagCount[t] = (tagCount[t] || 0) + 1;
            if (!books[bookId].tags.includes(t)) books[bookId].tags.push(t);
          });
          if (year) yearSet.add(year);

          // 统计批注数（> 开头的行）
          const noteMatches = body.match(/^>\s.*$/gm);
          if (noteMatches) books[bookId].noteCount += noteMatches.length;

        } catch (e) {
          console.warn(`读取摘抄文件失败: ${fullPath} — ${e.message}`);
        }
      }
    }
  }

  walkDir(EXCERPT_DIR, '');

  // 加载已保存的书籍状态
  const metaFile = path.join(EXCERPT_DIR, '..', 'excerpt-meta.json');
  let metaStatuses = {};
  if (fs.existsSync(metaFile)) {
    try { metaStatuses = JSON.parse(fs.readFileSync(metaFile, 'utf8')).statuses || {}; } catch {}
  }

  _excerptsCache = {
    books: Object.values(books).map(b => ({ ...b, status: metaStatuses[b.id] || 'unread' })),
    allTags: Object.entries(tagCount)
      .sort((a, b) => b[1] - a[1])
      .map(([name, count]) => ({ name, count })),
    years: [...yearSet].sort((a, b) => b - a),
    totalExcerpts,
  };
  _excerptsCacheTime = Date.now();
  return _excerptsCache;
}

/**
 * 解析摘抄正文（去掉 frontmatter，按 ## 拆分）
 */
function parseExcerptsFromFile(filePath) {
  try {
    const raw = fs.readFileSync(filePath, 'utf8');
    const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
    let fm = {};
    let body = '';

    if (fmMatch) {
      const fmText = fmMatch[1];
      const bookMatch = fmText.match(/book:\s*"([^"]+)"/);
      const authorMatch = fmText.match(/author:\s*"([^"]+)"/);
      const dateMatch = fmText.match(/date:\s*"([^"]+)"/);
      const tagsMatch = fmText.match(/tags:\s*\[([^\]]+)\]/);
      const favMatch = fmText.match(/favorite:\s*(true|false)/);

      if (bookMatch) fm.book = bookMatch[1];
      if (authorMatch) fm.author = authorMatch[1];
      if (dateMatch) fm.date = dateMatch[1];
      if (tagsMatch) fm.tags = tagsMatch[1].split(',').map(t => t.trim()).filter(t => t);
      if (favMatch) fm.favorite = favMatch[1] === 'true';
      body = raw.substring(fmMatch[0].length);
    } else {
      body = raw;
    }

    if (!fm.book) fm.book = path.basename(filePath, '.md');

    // 按 ## 标题拆分
    const sections = body.split(/^## /m).filter(s => s.trim());
    const excerpts = [];

    sections.forEach((section, idx) => {
      // section 格式: "标题\n\n内容" 或 "内容"（无标题时）
      const firstNewline = section.indexOf('\n');
      const title = firstNewline >= 0 ? section.substring(0, firstNewline).trim() : `摘抄 ${idx + 1}`;
      const content = firstNewline >= 0 ? section.substring(firstNewline + 1).trim() : section.trim();

      // 提取批注（> 开头的行）
      const noteLines = content.match(/^>\s*(?:\*\*批注\(Obsidian\)\*\*:\s*)?(.*)$/gm);
      const notes = noteLines ? noteLines.map(n => n.replace(/^>\s*(?:\*\*批注\(Obsidian\)\*\*:\s*)?/, '').trim()).filter(n => n) : [];
      // 提取正文（去掉批注行）
      const bodyText = content.replace(/^>\s*(?:\*\*批注\(Obsidian\)\*\*:\s*)?.*$/gm, '').trim();

      excerpts.push({
        id: `${sanitizeId(fm.book)}-${idx}`,
        book: fm.book,
        bookId: sanitizeId(fm.book).toLowerCase().replace(/\s+/g, '-'),
        body: bodyText,
        author: fm.author || '',
        date: fm.date || '',
        chapter: '',
        tags: fm.tags || [],
        notes: notes,
        favorite: fm.favorite || false,
        heading: title.replace(/[^a-zA-Z0-9一-鿿]/g, '-').toLowerCase(),
        mdFile: path.relative(ROOT, filePath).replace(/\\/g, '/'),
      });
    });

    // 如果没有 sections，整段作为一条摘抄
    if (excerpts.length === 0 && body.trim()) {
      excerpts.push({
        id: `${sanitizeId(fm.book)}-0`,
        book: fm.book,
        bookId: sanitizeId(fm.book).toLowerCase().replace(/\s+/g, '-'),
        body: body.trim(),
        author: fm.author || '',
        date: fm.date || '',
        chapter: '',
        tags: fm.tags || [],
        notes: [],
        favorite: fm.favorite || false,
        heading: '摘抄-1',
        mdFile: path.relative(ROOT, filePath).replace(/\\/g, '/'),
      });
    }

    return excerpts;
  } catch (e) {
    console.warn(`解析摘抄文件失败: ${filePath} — ${e.message}`);
    return [];
  }
}

/**
 * 全文搜索匹配
 */
function matchSearch(excerpt, query) {
  const q = query.toLowerCase();
  return excerpt.body.toLowerCase().includes(q)
    || excerpt.book.toLowerCase().includes(q)
    || excerpt.tags.some(t => t.toLowerCase().includes(q));
}

// 处理 /api/excerpts/*
function handleExcerptsAPI(req, res, route, method, body) {
  const type = route.type;

  // GET /api/excerpts/books
  if (type === 'excerpts-books' && method === 'GET') {
    const data = scanExcerpts();
    json(res, 200, data);
    return;
  }

  // GET /api/excerpts — 分页列表 + 搜索
  if (type === 'excerpts-list' && method === 'GET') {
    try {
      const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
      const book = url.searchParams.get('book');
      const tag = url.searchParams.get('tag');
      const year = url.searchParams.get('year');
      const hasNote = url.searchParams.get('has_note') === 'true';
      const favorite = url.searchParams.get('favorite') === 'true';
      const q = url.searchParams.get('q');
      let page = parseInt(url.searchParams.get('page')) || 1;
      let perPage = parseInt(url.searchParams.get('per_page')) || 20;
      if (page < 1) page = 1;
      if (perPage > 100) perPage = 100;

      // 扫描所有摘抄
      const allExcerpts = [];
      function walkDir(dir) {
        if (!fs.existsSync(dir)) return;
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          const fullPath = path.join(dir, entry.name);
          if (entry.isDirectory()) {
            walkDir(fullPath);
          } else if (entry.isFile() && entry.name.endsWith('.md')) {
            allExcerpts.push(...parseExcerptsFromFile(fullPath));
          }
        }
      }
      walkDir(EXCERPT_DIR);

      // 过滤
      let filtered = allExcerpts;
      if (book) filtered = filtered.filter(e => e.bookId === book);
      if (tag) filtered = filtered.filter(e => e.tags.includes(tag));
      if (year) filtered = filtered.filter(e => e.date && e.date.startsWith(year));
      if (hasNote) filtered = filtered.filter(e => e.notes.length > 0);
      if (favorite) filtered = filtered.filter(e => e.favorite);
      if (q) filtered = filtered.filter(e => matchSearch(e, q));

      // 分页
      const total = filtered.length;
      const totalPages = Math.max(1, Math.ceil(total / perPage));
      const start = (page - 1) * perPage;
      const pageExcerpts = filtered.slice(start, start + perPage);

      json(res, 200, {
        excerpts: pageExcerpts,
        pagination: { page, per_page: perPage, total, pages: totalPages },
      });
    } catch (e) {
      error(res, 500, '查询失败: ' + e.message);
    }
    return;
  }

  // PUT /api/excerpts/:id/meta
  if (type === 'excerpts-meta' && method === 'PUT') {
    try {
      const data = JSON.parse(body);
      // 查找对应的 .md 文件
      const bookId = route.id;
      let targetFile = null;
      function findFile(dir) {
        if (targetFile) return;
        if (!fs.existsSync(dir)) return;
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          const fullPath = path.join(dir, entry.name);
          if (entry.isDirectory()) {
            findFile(fullPath);
          } else if (entry.isFile() && entry.name.endsWith('.md')) {
            const raw = fs.readFileSync(fullPath, 'utf8');
            const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
            if (fmMatch) {
              const bookMatch = fmMatch[1].match(/book:\s*"([^"]+)"/);
              if (bookMatch && sanitizeId(bookMatch[1]).toLowerCase().replace(/\s+/g, '-') === bookId) {
                targetFile = fullPath;
                return;
              }
            }
          }
        }
      }
      findFile(EXCERPT_DIR);

      if (!targetFile) {
        error(res, 404, '摘抄未找到');
        return;
      }

      // 更新 frontmatter
      const raw = fs.readFileSync(targetFile, 'utf8');
      const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
      if (fmMatch) {
        let fm = fmMatch[1];
        if (data.book) fm = fm.replace(/book:\s*".*?"/, `book: "${data.book}"`);
        if (data.author !== undefined) fm = fm.replace(/author:\s*".*?"/, `author: "${data.author || ''}"`);
        if (data.date) fm = fm.replace(/date:\s*".*?"/, `date: "${data.date}"`);
        if (data.tags) fm = fm.replace(/tags:\s*\[[^\]]*\]/, `tags: [${data.tags.join(', ')}]`);
        if (data.favorite !== undefined) fm = fm.replace(/favorite:\s*(true|false)/, `favorite: ${data.favorite}`);
        if (data.chapter) fm = fm.replace(/chapter:\s*".*?"/, `chapter: "${data.chapter}"`);
        else if (data.chapter !== undefined) fm += `\nchapter: "${data.chapter}"`;
        if (data.page) fm = fm.replace(/page:\s*".*?"/, `page: "${data.page}"`);
        else if (data.page !== undefined) fm += `\npage: "${data.page}"`;

        const newRaw = raw.substring(0, fmMatch.index) + '---\n' + fm + '\n---\n' + raw.substring(fmMatch.index + fmMatch[0].length);
        fs.writeFileSync(targetFile, newRaw, 'utf8');
        // 清除缓存
        _excerptsCache = null;
      }

      json(res, 200, { success: true, updated: path.relative(ROOT, targetFile) });
    } catch (e) {
      error(res, 500, '更新失败: ' + e.message);
    }
    return;
  }

  // POST /api/excerpts/:id/note
  if (type === 'excerpts-note' && method === 'POST') {
    try {
      const data = JSON.parse(body);
      const bookId = route.id;
      let targetFile = null;
      function findFile(dir) {
        if (targetFile) return;
        if (!fs.existsSync(dir)) return;
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          const fullPath = path.join(dir, entry.name);
          if (entry.isDirectory()) findFile(fullPath);
          else if (entry.isFile() && entry.name.endsWith('.md')) {
            const raw = fs.readFileSync(fullPath, 'utf8');
            const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
            if (fmMatch) {
              const bookMatch = fmMatch[1].match(/book:\s*"([^"]+)"/);
              if (bookMatch && sanitizeId(bookMatch[1]).toLowerCase().replace(/\s+/g, '-') === bookId) {
                targetFile = fullPath;
                return;
              }
            }
          }
        }
      }
      findFile(EXCERPT_DIR);

      if (!targetFile) {
        error(res, 404, '摘抄未找到');
        return;
      }

      // 在文件末尾追加批注
      const noteLine = `\n> **批注(${data.source || 'html'})**: ${data.content}`;
      fs.appendFileSync(targetFile, noteLine, 'utf8');
      json(res, 200, { success: true });
    } catch (e) {
      error(res, 500, '添加批注失败: ' + e.message);
    }
    return;
  }

  // GET /api/excerpts/:id/notes
  if (type === 'excerpts-notes' && method === 'GET') {
    // 简化：从 parseExcerptsFromFile 的结果中获取
    // 实际实现需要更精细的文件定位
    json(res, 200, { notes: [] });
    return;
  }

  // PUT /api/excerpts/:id/favorite
  if (type === 'excerpts-favorite' && method === 'PUT') {
    try {
      const data = JSON.parse(body);
      const bookId = route.id;
      // 复用上面的文件查找逻辑
      let targetFile = null;
      function findFile(dir) {
        if (targetFile) return;
        if (!fs.existsSync(dir)) return;
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          const fullPath = path.join(dir, entry.name);
          if (entry.isDirectory()) findFile(fullPath);
          else if (entry.isFile() && entry.name.endsWith('.md')) {
            const raw = fs.readFileSync(fullPath, 'utf8');
            const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
            if (fmMatch) {
              const bookMatch = fmMatch[1].match(/book:\s*"([^"]+)"/);
              if (bookMatch && sanitizeId(bookMatch[1]).toLowerCase().replace(/\s+/g, '-') === bookId) {
                targetFile = fullPath;
                return;
              }
            }
          }
        }
      }
      findFile(EXCERPT_DIR);

      if (!targetFile) {
        error(res, 404, '摘抄未找到');
        return;
      }

      const raw = fs.readFileSync(targetFile, 'utf8');
      const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
      if (fmMatch) {
        let fm = fmMatch[1];
        fm = fm.replace(/favorite:\s*(true|false)/, `favorite: ${data.favorite}`);
        const newRaw = raw.substring(0, fmMatch.index) + '---\n' + fm + '\n---\n' + raw.substring(fmMatch.index + fmMatch[0].length);
        fs.writeFileSync(targetFile, newRaw, 'utf8');
        _excerptsCache = null;
      }
      json(res, 200, { success: true });
    } catch (e) {
      error(res, 500, '收藏更新失败: ' + e.message);
    }
    return;
  }

  // PUT /api/excerpts/books/:id/status — 更新书籍状态
  if (type === 'excerpts-book-status' && method === 'PUT') {
    try {
      const data = JSON.parse(body);
      const bookId = route.id;
      const newStatus = data.status;
      const validStatuses = ['unread', 'want', 'reading', 'done', 'abandoned'];
      if (!validStatuses.includes(newStatus)) {
        error(res, 400, '无效的状态值');
        return;
      }

      // 状态存储在 localStorage 的元数据文件中
      const metaFile = path.join(EXCERPT_DIR, '..', 'excerpt-meta.json');
      let meta = {};
      if (fs.existsSync(metaFile)) {
        try { meta = JSON.parse(fs.readFileSync(metaFile, 'utf8')); } catch {}
      }
      if (!meta.statuses) meta.statuses = {};
      meta.statuses[bookId] = newStatus;
      fs.writeFileSync(metaFile, JSON.stringify(meta, null, 2), 'utf8');

      // 清除缓存，使下次请求重新扫描
      _excerptsCache = null;

      json(res, 200, { success: true, status: newStatus });
    } catch (e) {
      error(res, 500, '状态更新失败: ' + e.message);
    }
    return;
  }

  // DELETE /api/excerpts/:id — 删除摘抄段落
  if (type === 'excerpts-item' && method === 'DELETE') {
    try {
      const excerptId = route.id;
      // 解析 excerptId 找出对应的 bookId 和段落索引
      const dashIdx = excerptId.lastIndexOf('-');
      if (dashIdx < 0) { error(res, 400, '无效的摘抄ID'); return; }
      const bookId = excerptId.substring(0, dashIdx);
      const excerptIdx = parseInt(excerptId.substring(dashIdx + 1));

      // 查找目标文件
      let targetFile = null;
      let _found = false;
      function findTargetFile(dir) {
        if (_found) return;
        if (!fs.existsSync(dir)) return;
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          if (_found) return;
          const fullPath = path.join(dir, entry.name);
          if (entry.isDirectory()) findTargetFile(fullPath);
          else if (entry.isFile() && entry.name.endsWith('.md')) {
            const raw = fs.readFileSync(fullPath, 'utf8');
            const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
            if (fmMatch) {
              const bookMatch = fmMatch[1].match(/book:\s*"([^"]+)"/);
              if (bookMatch && sanitizeId(bookMatch[1]).toLowerCase().replace(/\s+/g, '-') === bookId) {
                targetFile = fullPath;
                _found = true;
                return;
              }
            }
          }
        }
      }
      findTargetFile(EXCERPT_DIR);

      if (!targetFile) { error(res, 404, '未找到文件'); return; }

      // 读取文件，按 ## 摘抄 拆分
      const raw = fs.readFileSync(targetFile, 'utf8');
      const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
      const frontmatter = fmMatch ? fmMatch[0] : '';
      const body = raw.substring(frontmatter.length);

      // 找到目标段落
      const sections = body.split(/^## /m);
      if (excerptIdx < 0 || excerptIdx >= sections.length - 1) {
        error(res, 404, '摘抄段落不存在'); return;
      }

      // 删除目标段落（保留 ## 标题行，删除整个 section）
      const targetSection = sections[excerptIdx + 1];
      const targetRegex = new RegExp('(\\n|^)## ' + escapeRegex(targetSection.split('\n')[0]) + '[\\s\\S]*?(?=\\n## |$)', 'm');

      // 更简单的方式：重建 sections 数组，排除目标
      const newSections = sections.filter((_, i) => i === 0 || (i - 1) !== excerptIdx);
      const newBody = newSections.join('');
      const newRaw = frontmatter + newBody;

      fs.writeFileSync(targetFile, newRaw, 'utf8');
      // 清除缓存
      _excerptsCache = null;
      json(res, 200, { success: true });
    } catch (e) {
      error(res, 500, '删除失败: ' + e.message);
    }
    return;
  }

  // POST /api/excerpts/:id — 在指定书中新增摘抄段落
  if (type === 'excerpts-item' && method === 'POST') {
    try {
      const data = JSON.parse(body);
      const bookId = route.id;
      const title = data.title || '新摘抄';
      const content = data.content || '';

      // 查找目标文件
      let targetFile = null;
      function findTargetFile(dir) {
        if (targetFile) return;
        if (!fs.existsSync(dir)) return;
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          const fullPath = path.join(dir, entry.name);
          if (entry.isDirectory()) findTargetFile(fullPath);
          else if (entry.isFile() && entry.name.endsWith('.md')) {
            const raw = fs.readFileSync(fullPath, 'utf8');
            const fmMatch = raw.match(/^---\n([\s\S]*?)\n---\n?/);
            if (fmMatch) {
              const bookMatch = fmMatch[1].match(/book:\s*"([^"]+)"/);
              if (bookMatch && sanitizeId(bookMatch[1]).toLowerCase().replace(/\s+/g, '-') === bookId) {
                targetFile = fullPath;
                return;
              }
            }
          }
        }
      }
      findTargetFile(EXCERPT_DIR);

      if (!targetFile) { error(res, 404, '未找到文件'); return; }

      // 追加新摘抄段落
      const newSection = `\n## ${title}\n\n${content}\n`;
      fs.appendFileSync(targetFile, newSection, 'utf8');
      // 清除缓存
      _excerptsCache = null;
      json(res, 200, { success: true });
    } catch (e) {
      error(res, 500, '新增失败: ' + e.message);
    }
    return;
  }

  // GET /api/excerpts/suggestions
  if (type === 'excerpts-suggestions' && method === 'GET') {
    try {
      const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
      const type = url.searchParams.get('type'); // 'tag' | 'book'
      const q = url.searchParams.get('q') || '';
      const data = scanExcerpts();

      if (type === 'tag') {
        const matches = data.allTags
          .filter(t => t.name.includes(q))
          .slice(0, 10);
        json(res, 200, matches);
      } else if (type === 'book') {
        const matches = data.books
          .filter(b => b.title.includes(q))
          .slice(0, 10);
        json(res, 200, matches);
      } else {
        error(res, 400, '不支持的建议类型');
      }
    } catch (e) {
      error(res, 500, '搜索建议失败: ' + e.message);
    }
    return;
  }

  // GET /api/excerpts/export
  if (type === 'excerpts-export' && method === 'GET') {
    try {
      const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
      const format = url.searchParams.get('format') || 'json';

      // 扫描所有摘抄
      const allExcerpts = [];
      function walkDir(dir) {
        if (!fs.existsSync(dir)) return;
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        for (const entry of entries) {
          const fullPath = path.join(dir, entry.name);
          if (entry.isDirectory()) walkDir(fullPath);
          else if (entry.isFile() && entry.name.endsWith('.md')) {
            allExcerpts.push(...parseExcerptsFromFile(fullPath));
          }
        }
      }
      walkDir(EXCERPT_DIR);

      if (format === 'json') {
        res.writeHead(200, {
          'Content-Type': 'application/json; charset=utf-8',
          'Content-Disposition': 'attachment; filename="excerpts.json"',
        });
        res.end(JSON.stringify(allExcerpts, null, 2));
      } else if (format === 'markdown') {
        let md = '# 技术摘抄导出\n\n';
        allExcerpts.forEach(e => {
          md += `## ${e.book} — ${e.heading || '摘抄'}\n\n${e.body}\n\n`;
          if (e.tags.length) md += `Tags: ${e.tags.join(', ')}\n\n`;
          md += '---\n\n';
        });
        res.writeHead(200, {
          'Content-Type': 'text/markdown; charset=utf-8',
          'Content-Disposition': 'attachment; filename="excerpts.md"',
        });
        res.end(md);
      } else {
        error(res, 400, '不支持的导出格式');
      }
    } catch (e) {
      error(res, 500, '导出失败: ' + e.message);
    }
    return;
  }

  error(res, 404, '未知的摘抄端点');
}

// ============================================================
// 请求路由
// ============================================================
function handleRequest(req, res) {
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const pathname = decodeURIComponent(url.pathname);

  // CORS 预检
  if (req.method === 'OPTIONS') {
    return handleCORS(res);
  }

  // API 路由
  if (pathname.startsWith('/api/')) {
    const route = matchRoute(pathname);

    if (!route) {
      return error(res, 404, 'API 端点不存在');
    }

    // 读取请求体（仅 PUT/POST 需要）
    if (['PUT', 'POST'].includes(req.method)) {
      // 对于文件上传，不预先读取 body
      if (req.headers['content-type'] && req.headers['content-type'].includes('multipart/form-data')) {
        return dispatchAPI(req, res, route);
      }
    }

    // 读取 JSON/文本 body
    let body = '';
    req.setEncoding('utf8');
    req.on('data', chunk => { body += chunk; });
    req.on('end', () => dispatchAPI(req, res, route, body));
    return;
  }

  // 非 API — 静态文件
  serveStatic(req, res, pathname);
}

function dispatchAPI(req, res, route, body) {
  const { method } = req;
  const { type } = route;

  // ============ 状态读写 ============
  if (type === 'state') {
    if (method === 'GET') return handleGetState(res, route.app);
    if (method === 'PUT') return handlePutState(res, route.app, body || '{}');
    return error(res, 405, '方法不允许');
  }

  // ============ PDF 操作 ============
  if (type === 'book-pdf') {
    if (method === 'GET') return handleGetPDF(res, route.mode, route.bookId);
    if (method === 'POST') return handlePostPDF(res, route.mode, route.bookId, req);
    if (method === 'DELETE') return handleDeletePDF(res, route.mode, route.bookId);
    return error(res, 405, '方法不允许');
  }

  // ============ 项目文件操作 (上传多个) ============
  if (type === 'project-files') {
    if (method === 'POST') return handlePostProjectFiles(res, route.catId, route.projId, req);
    return error(res, 405, '方法不允许');
  }

  // ============ 单个项目文件 ============
  if (type === 'project-file') {
    if (method === 'GET') return handleGetProjectFile(res, route.catId, route.projId, route.filename);
    if (method === 'DELETE') return handleDeleteProjectFile(res, route.catId, route.projId, route.filename);
    return error(res, 405, '方法不允许');
  }

  // ============ 面试追踪 ============
  if (type.startsWith('tracker')) {
    return handleTrackerAPI(req, res, route, method, body);
  }

  // ============ 技术摘抄 ============
  if (type.startsWith('excerpts')) {
    return handleExcerptsAPI(req, res, route, method, body);
  }

  error(res, 404, '未知 API');
}

// ============================================================
// 启动服务器
// ============================================================
const server = http.createServer(handleRequest);

server.listen(PORT, '0.0.0.0', () => {
  const ips = getLocalIPs();

  console.log('');
  console.log('  ╔══════════════════════════════════════╗');
  console.log('  ║  📡 读书雷达服务已启动             ║');
  console.log('  ╚══════════════════════════════════════╝');
  console.log('');
  console.log(`    本地访问:   http://localhost:${PORT}`);
  for (const ip of ips) {
    console.log(`    局域网访问:  http://${ip}:${PORT}`);
  }
  console.log(`    数据目录:    ${path.relative(process.cwd(), DATA_DIR)}/`);
  console.log('');
  console.log('  提示: 按 Ctrl+C 停止服务');
  console.log('');
});

// 优雅关闭
process.on('SIGINT', () => {
  console.log('\n服务器已停止。');
  process.exit(0);
});
