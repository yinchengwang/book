---
name: storage-engine-stack-overflow
description: Pre-existing STATUS_STACK_OVERFLOW (0xC00000FD) in storage engine kv_put/WAL path — discovered while building github-issue-todo
metadata: 
  node_type: memory
  type: project
  originSessionId: d064c811-a326-4200-899d-f8fdac83fe11
---

# Storage engine has pre-existing stack overflow

**Why:** While implementing `engineering/apps/github-issue-todo` (relational engine powered issue tracker), even minimal `kv_put` triggers `STATUS_STACK_OVERFLOW` on Windows. Verified via standalone test programs that:
- `kv_open` works fine on a brand-new DB file
- `table_create` (which calls no `kv_put`) works fine
- `kv_put` with any key/value crashes immediately, exit code `-1073741571` = `0xC00000FD`

The crash happens BEFORE any debug output after `kv_put` is called — too early to pinpoint the call site with fprintf. Suspect WAL or buffer pool, not the json file path.

**Workaround used:** `github-issue-todo` switched to JSON file persistence via cJSON, dropping `db_storage`/`db_core` deps. API signatures kept compatible (`issue_t`/`checklist_item_t`/`issue_query_t` etc.) so we can swap back when fixed. See commits 6b79f04 ("实现后端 API + JSON 文件存储") and on.

**How to apply:**
- Do NOT add new apps depending on `db_storage` until this bug is rooted out — every `kv_put` will crash
- Bug hunt candidates: `engineering/src/db/storage/wal/wal.c` (`wal_write_record`), `wal_buf.c`, `kv.c` (`kv_put`), `bufmgr.c`
- Stack overflow in pure C often means unbounded recursion, deep call chain via macros, or VLA / huge local buffer — none of `kv_put`'s visible locals are huge so likely deep call chain
- When testing small storage apps: first prove `kv_open` + simplest `kv_put` on a freshly created DB file works; if it crashes, use JSON file fallback rather than losing days debugging
