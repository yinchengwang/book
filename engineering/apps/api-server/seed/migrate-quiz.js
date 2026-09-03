/**
 * migrate-quiz.js
 *
 * Migrate quiz data from reading-radar JS files to book.db.
 * File format: QUESTION_BANK.stack (Object.assign) { "quadrant": [ {...}, ... ] }
 *
 * Usage: bun migrate-quiz.js <book.db> <reading-radar-data-dir>
 */

import { readFileSync, readdirSync, existsSync } from 'fs';
import { join } from 'path';
import { Database } from 'bun:sqlite';

const dbPath = process.argv[2];
const dataDir = process.argv[3];
if (!dbPath || !dataDir) {
  console.error('Usage: bun migrate-quiz.js <book.db> <reading-radar-data-dir>');
  process.exit(1);
}

function extractQuadrantArrays(jsContent) {
  const results = [];
  const re = /"(\w+)":\s*\[/g;
  let m;
  while ((m = re.exec(jsContent)) !== null) {
    const quadrant = m[1];
    let depth = 1;
    let i = m.index + m[0].length;
    const start = i;
    while (i < jsContent.length && depth > 0) {
      if (jsContent[i] === '[') depth++;
      else if (jsContent[i] === ']') depth--;
      if (depth > 0) i++;
    }
    results.push({ quadrant, arrayContent: jsContent.substring(start, i) });
  }
  return results;
}

function extractObjects(arrayContent) {
  const objects = [];
  let oi = 0;
  while (oi < arrayContent.length) {
    while (oi < arrayContent.length && /[\s,]/.test(arrayContent[oi])) oi++;
    if (oi >= arrayContent.length || arrayContent[oi] !== '{') break;
    let depth = 1, oj = oi + 1;
    while (oj < arrayContent.length && depth > 0) {
      if (arrayContent[oj] === '{') depth++;
      else if (arrayContent[oj] === '}') depth--;
      if (depth > 0) oj++;
    }
    objects.push(arrayContent.substring(oi, oj + 1));
    oi = oj + 1;
  }
  return objects;
}

function litToJson(str) {
  str = str.trim();
  if ((str.startsWith("'") && str.endsWith("'")) || (str.startsWith('"') && str.endsWith('"')))
    return JSON.stringify(str.slice(1, -1));
  if (str === 'true' || str === 'false' || str === 'null') return str;
  if (/^-?\d+(\.\d+)?$/.test(str)) return str;
  if (str.startsWith('[') && str.endsWith(']')) {
    const inner = str.slice(1, -1).trim();
    if (!inner) return '[]';
    const items = [];
    let ci = 0, depth = 0, cur = '';
    while (ci < inner.length) {
      const ch = inner[ci];
      if ((ch === ',' || ch === '\n') && depth === 0) {
        if (cur.trim()) items.push(litToJson(cur.trim()));
        cur = '';
      } else {
        if (ch === '{' || ch === '[') depth++;
        else if (ch === '}' || ch === ']') depth--;
        cur += ch;
      }
      ci++;
    }
    if (cur.trim()) items.push(litToJson(cur.trim()));
    return '[' + items.join(',') + ']';
  }
  return JSON.stringify(str);
}

function jsObjToJson(jsStr) {
  let s = jsStr.trim();
  if (s.startsWith('{') && s.endsWith('}')) s = s.slice(1, -1).trim();
  const pairs = [];
  let ci = 0, depth = 0, cur = '';
  while (ci < s.length) {
    const ch = s[ci];
    if ((ch === ',' || ch === '\n') && depth === 0) {
      if (cur.trim()) pairs.push(cur.trim());
      cur = '';
    } else {
      if (ch === '{' || ch === '[') depth++;
      else if (ch === '}' || ch === ']') depth--;
      cur += ch;
    }
    ci++;
  }
  if (cur.trim()) pairs.push(cur.trim());

  const parts = [];
  for (const pair of pairs) {
    const colon = pair.indexOf(':');
    if (colon < 0) continue;
    let key = pair.substring(0, colon).trim();
    let val = pair.substring(colon + 1).trim();
    if ((key.startsWith("'") && key.endsWith("'")) || (key.startsWith('"') && key.endsWith('"')))
      key = key.slice(1, -1);
    parts.push(JSON.stringify(key) + ':' + litToJson(val));
  }
  return '{' + parts.join(',') + '}';
}

function main() {
  console.log('=== Quiz Migration ===\n');

  const db = new Database(dbPath);

  const createSql = 'CREATE TABLE IF NOT EXISTS quiz_questions (' +
    'id TEXT PRIMARY KEY, stack TEXT NOT NULL, title TEXT NOT NULL, ' +
    'category TEXT NOT NULL DEFAULT "", difficulty TEXT NOT NULL DEFAULT "", ' +
    'options TEXT, answer TEXT NOT NULL, explanation TEXT, ' +
    'tags TEXT NOT NULL DEFAULT "[]", time_estimate INTEGER DEFAULT 0, ' +
    'created_at TEXT NOT NULL)';
  db.run(createSql);

  const stacks = ['c', 'cpp', 'db', 'ds', 'linux', 'py', 'vdb'];
  const diffMap = { basic: '入门', beginner: '入门', easy: '初级', intermediate: '中级', medium: '中级', advanced: '高级', hard: '高级', expert: '专家' };
  const typeMap = { choice: '选择题', true_false: '判断题', predict_output: '简答题', fill_blank: '填空题', coding: '编程题', short_answer: '简答题', multi_choice: '多选题' };

  const insertSql = 'INSERT OR IGNORE INTO quiz_questions (id, stack, title, category, difficulty, options, answer, explanation, tags, time_estimate, created_at) VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,datetime(\'now\'))';
  const insert = db.prepare(insertSql);

  const tx = db.transaction((items) => {
    for (const q of items) insert.run(q.id, q.stack, q.title, q.category, q.difficulty, q.options, q.answer, q.explanation, q.tags, q.time_estimate);
  });

  let total = 0;

  for (const stack of stacks) {
    const quizDir = join(dataDir, 'data', 'quiz', 'questions', stack);
    if (!existsSync(quizDir)) { console.log('  ' + stack + ': dir not found'); continue; }
    console.log('\nProcessing ' + stack + ':');

    const files = readdirSync(quizDir).filter(f => f.endsWith('.js'));
    const batch = [];

    for (const file of files) {
      const content = readFileSync(join(quizDir, file), 'utf-8');
      const quadrants = extractQuadrantArrays(content);
      let fileCount = 0;

      for (const { quadrant, arrayContent } of quadrants) {
        const objects = extractObjects(arrayContent);
        for (const objStr of objects) {
          try {
            const jsonStr = jsObjToJson(objStr);
            const obj = JSON.parse(jsonStr);
            if (!obj.stem && !obj.title) continue;

            let optionsStr = null;
            if (Array.isArray(obj.options))
              optionsStr = JSON.stringify(obj.options.map(o => typeof o === 'string' ? o.replace(/^[A-Za-z][.．\s]*/, '') : o));
            else if (typeof obj.options === 'string' && obj.options) optionsStr = obj.options;

            let answerStr = String(obj.answer);
            if (answerStr === 'true') answerStr = '正确';
            else if (answerStr === 'false') answerStr = '错误';

            const tagsArr = [];
            if (Array.isArray(obj.tags)) tagsArr.push.apply(tagsArr, obj.tags);
            else if (obj.tags) tagsArr.push(String(obj.tags));
            tagsArr.push(stack, quadrant);
            const uniqueTags = [];
            for (const t of tagsArr) { if (uniqueTags.indexOf(t) < 0) uniqueTags.push(t); }

            batch.push({
              id: obj.id || (stack + '_' + quadrant + '_' + fileCount),
              stack: stack,
              title: obj.stem || obj.title || '',
              category: typeMap[obj.type] || '简答题',
              difficulty: diffMap[(obj.difficulty || '').toLowerCase()] || obj.difficulty || '',
              options: optionsStr,
              answer: answerStr,
              explanation: obj.explanation || '',
              tags: JSON.stringify(uniqueTags),
              time_estimate: obj.timeEstimate || obj.time_estimate || 0
            });
            fileCount++;
          } catch (e) { /* skip parse failures */ }
        }
      }
      console.log('  ' + file + ': ' + fileCount);
    }

    if (batch.length > 0) tx(batch);
    total += batch.length;
    console.log('  Stack total: ' + batch.length);
  }

  db.close();
  console.log('\nDone! Migrated ' + total + ' quiz questions.');
}

main();
