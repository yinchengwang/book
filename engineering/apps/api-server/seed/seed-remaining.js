import { readFileSync, readdirSync, existsSync } from 'fs';
import { join } from 'path';
import { Database } from 'bun:sqlite';

const dbPath = process.argv[2];
const dataDir = process.argv[3];
const notesRoot = process.argv[4] || join(process.cwd(), 'learning/notes');

if (!dbPath || !dataDir) {
  console.error('Usage: bun seed-remaining.js <book.db> <reading-radar-data-dir>');
  process.exit(1);
}

const db = new Database(dbPath);
db.run('PRAGMA journal_mode=WAL');

function createTable(name, ddl) {
  const sql = 'CREATE TABLE IF NOT EXISTS ' + name + ' (' + ddl + ')';
  db.run(sql);
}

createTable('interview_questions',
  'id TEXT PRIMARY KEY, category TEXT NOT NULL, title TEXT NOT NULL, ' +
  'body TEXT NOT NULL, tags TEXT NOT NULL DEFAULT "[]", created_at TEXT NOT NULL');

createTable('interview_tracker',
  'id TEXT PRIMARY KEY, company TEXT NOT NULL, position TEXT, ' +
  'status TEXT NOT NULL DEFAULT "screening", salary TEXT, notes TEXT, ' +
  'created_at TEXT NOT NULL, updated_at TEXT NOT NULL');

createTable('interview_rounds',
  'id INTEGER PRIMARY KEY AUTOINCREMENT, tracker_id TEXT NOT NULL, ' +
  'round_type TEXT NOT NULL, round_date TEXT NOT NULL, content TEXT, created_at TEXT NOT NULL');

createTable('notes_meta',
  'id TEXT PRIMARY KEY, file_path TEXT NOT NULL UNIQUE, ' +
  'title TEXT NOT NULL DEFAULT "", body TEXT NOT NULL DEFAULT "", ' +
  'tags TEXT NOT NULL DEFAULT "[]", context TEXT NOT NULL DEFAULT "", ' +
  'source TEXT NOT NULL DEFAULT "", parent_dir TEXT NOT NULL DEFAULT "", ' +
  'status TEXT NOT NULL DEFAULT "", rating INTEGER NOT NULL DEFAULT 0, ' +
  'favorite INTEGER NOT NULL DEFAULT 0, word_count INTEGER NOT NULL DEFAULT 0, ' +
  'created_at TEXT NOT NULL, updated_at TEXT NOT NULL');

createTable('dir_tree',
  'path TEXT PRIMARY KEY, note_count INTEGER NOT NULL DEFAULT 0, updated_at TEXT NOT NULL');

function now() {
  const d = new Date();
  return d.toISOString().replace('T', 'T').substring(0, 19);
}

// ===== 1. Interview questions =====

function migrateInterviewQuestions(dir, category) {
  const entries = readdirSync(dir, { withFileTypes: true });
  let count = 0;

  for (const entry of entries) {
    if (entry.isDirectory() && entry.name !== '.' && entry.name !== '..') {
      count += migrateInterviewQuestions(join(dir, entry.name), category ? category + '/' + entry.name : entry.name);
    } else if (entry.isFile() && entry.name.endsWith('.md')) {
      const fp = join(dir, entry.name);
      const content = readFileSync(fp, 'utf-8');

      let title = entry.name.replace(/\.md$/, '');
      let body = content;
      let tags = '[]';

      if (content.startsWith('---')) {
        const endIdx = content.indexOf('\n---', 3);
        if (endIdx > 0) {
          const frontmatter = content.substring(3, endIdx);
          body = content.substring(endIdx + 4).trimStart();
          const tMatch = frontmatter.match(/title:\s*"?([^"\n]+)"?/);
          if (tMatch) title = tMatch[1];
          const tagMatch = frontmatter.match(/tags:\s*(\[.*?\])\s*[\n]/);
          if (tagMatch) tags = tagMatch[1];
        }
      }

      const id = 'iq_' + (category ? category.replace(/[/\\]/g, '_') + '_' : '') + title.replace(/[^a-zA-Z0-9_]/g, '_');

      try {
        db.run('INSERT OR IGNORE INTO interview_questions (id, category, title, body, tags, created_at) VALUES (?1,?2,?3,?4,?5,?6)',
          id, category || '', title, body, tags, now());
        count++;
      } catch (e) {
        console.error('  Error inserting ' + fp + ': ' + e.message);
      }
    }
  }
  return count;
}

// ===== 2. Interview tracker =====

function migrateInterviewTracker(dir) {
  const indexPath = join(dir, '_index.json');
  if (!existsSync(indexPath)) {
    console.log('  No _index.json found');
    return 0;
  }

  const content = readFileSync(indexPath, 'utf-8');
  let data;
  try { data = JSON.parse(content); } catch (e) { console.error('  Invalid JSON'); return 0; }

  let count = 0;
  for (const company of data.companies || []) {
    const id = 'tr_' + (company.companyId || company.id || 'unknown');
    try {
      db.run('INSERT OR IGNORE INTO interview_tracker (id, company, position, status, created_at, updated_at) VALUES (?1,?2,?3,?4,?5,?6)',
        id, company.name || company.company || company.companyId || '', company.position || '',
        company.status || 'applied', now(), now());

      for (const round of company.rounds || []) {
        try {
          db.run('INSERT INTO interview_rounds (tracker_id, round_type, round_date, content, created_at) VALUES (?1,?2,?3,?4,?5)',
            id, round.type || round.round_type || 'unknown', round.date || round.round_date || now(),
            round.content || round.notes || '', now());
        } catch (e) { /* skip */ }
      }
      count++;
    } catch (e) {
      console.error('  Error inserting tracker: ' + e.message);
    }
  }
  return count;
}

// ===== 3. Excerpts =====

function migrateExcerpts(excerptDir) {
  if (!existsSync(excerptDir)) { console.log('  Excerpt dir not found'); return 0; }

  const years = readdirSync(excerptDir, { withFileTypes: true }).filter(d => d.isDirectory() && d.name !== '.' && d.name !== '..');
  let count = 0;

  for (const yearDir of years) {
    const yearPath = join(excerptDir, yearDir.name);
    const files = readdirSync(yearPath).filter(f => f.endsWith('.md'));

    for (const file of files) {
      const fp = join(yearPath, file);
      const content = readFileSync(fp, 'utf-8');

      let title = file.replace(/\.md$/, '');
      let body = content;

      if (content.startsWith('---')) {
        const endIdx = content.indexOf('\n---', 3);
        if (endIdx > 0) {
          const frontmatter = content.substring(3, endIdx);
          body = content.substring(endIdx + 4).trimStart();
          const tMatch = frontmatter.match(/title:\s*"?([^"\n]+)"?/);
          if (tMatch) title = tMatch[1];
        }
      }

      const id = 'ex_' + yearDir.name + '_' + file.replace(/\.md$/, '').replace(/[^a-zA-Z0-9_]/g, '_');
      const filePath = 'excerpt/' + yearDir.name + '/' + file;

      try {
        db.run('INSERT OR IGNORE INTO notes_meta (id, file_path, title, body, tags, context, source, created_at, updated_at, word_count) VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)',
          id, filePath, title, body, '[]', 'excerpt', file, now(), now(), body.length);
        count++;
      } catch (e) {
        console.error('  Error inserting excerpt: ' + e.message);
      }
    }
  }
  return count;
}

// ===== Main =====

function main() {
  console.log('=== Seed Remaining Data ===\n');

  console.log('1. Interview questions...');
  const iqDir = join(dataDir, 'data', 'interview-questions');
  if (existsSync(iqDir)) {
    const n = migrateInterviewQuestions(iqDir, '');
    console.log('  Migrated: ' + n + ' questions');
  } else {
    console.log('  Directory not found: ' + iqDir);
  }

  console.log('\n2. Interview tracker...');
  const itDir = join(dataDir, 'data', 'interview-tracker');
  if (existsSync(itDir)) {
    const n = migrateInterviewTracker(itDir);
    console.log('  Migrated: ' + n + ' trackers');
  } else {
    console.log('  Directory not found: ' + itDir);
  }

  console.log('\n3. Excerpts to notes_meta...');
  const exDir = join(dataDir, 'data', 'excerpt');
  if (existsSync(exDir)) {
    const n = migrateExcerpts(exDir);
    console.log('  Migrated: ' + n + ' excerpts');
  } else {
    console.log('  Directory not found: ' + exDir);
  }

  db.close();
  console.log('\nDone!');
}

main();
