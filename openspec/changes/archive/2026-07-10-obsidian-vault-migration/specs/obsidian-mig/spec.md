# P4 Spec —— Obsidian Vault 完整化

## 1. _dashboard/progress.md 契约

含 3 个 dataview 块：
- 总览（每轨道笔记数）
- 最近 7 天笔记列表
- 完整 vault 统计

## 2. _index/ 轨道索引契约

每个轨道：
- `c.md` / `cpp.md` / `db.md` / `ds.md` / `grok.md` / `linux.md`
- 简介（~200 字）
- dataview 列表
- 关键代码链接

## 3. .obsidian/app.json 校验

含：
- community plugins enabled: dataview, templater, backlinks
- dailyNote.enabled: true

## 4. 不做

- ❌ 笔记内容修改
- ❌ 替换 vault
- ❌ 商业 plugins
