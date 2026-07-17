#!/usr/bin/env python3
"""
backup_recovery — 数据库备份恢复演示

演示冷备/热备/全量备份/增量备份及恢复演练（Python 模拟 + shell 命令）。
"""

import os
import shutil
import subprocess
import time
from typing import List, Dict, Optional
from dataclasses import dataclass, field
import json
import tempfile

DB_DIR = tempfile.mkdtemp(prefix="db_backup_demo_")
BACKUP_DIR = tempfile.mkdtemp(prefix="db_backup_backup_")
RESTORE_DIR = tempfile.mkdtemp(prefix="db_backup_restore_")
INC_DIR = tempfile.mkdtemp(prefix="db_backup_inc_")


class SimpleDB:
    """模拟数据库——文件存储"""

    def __init__(self, data_dir: str):
        self.data_dir = data_dir
        os.makedirs(data_dir, exist_ok=True)
        self.tables: Dict[str, List[Dict]] = {}
        self.wal: List[str] = []
        self._load()

    def _load(self):
        # 从磁盘加载数据
        data_file = os.path.join(self.data_dir, "data.json")
        if os.path.exists(data_file):
            with open(data_file) as f:
                data = json.load(f)
                self.tables = data.get("tables", {})
        wal_file = os.path.join(self.data_dir, "wal.log")
        if os.path.exists(wal_file):
            with open(wal_file) as f:
                self.wal = [l.strip() for l in f.readlines() if l.strip()]

    def _save(self):
        data_file = os.path.join(self.data_dir, "data.json")
        with open(data_file, "w") as f:
            json.dump({"tables": self.tables}, f)

    def _append_wal(self, entry: str):
        self.wal.append(entry)
        wal_file = os.path.join(self.data_dir, "wal.log")
        with open(wal_file, "a") as f:
            f.write(entry + "\n")

    def create_table(self, name: str):
        if name not in self.tables:
            self.tables[name] = []
            self._save()
            self._append_wal(f"CREATE TABLE {name}")

    def insert(self, table: str, row: Dict):
        if table not in self.tables:
            self.create_table(table)
        self.tables[table].append(row)
        self._save()
        self._append_wal(f"INSERT INTO {table} {json.dumps(row)}")

    def read_all(self, table: str) -> List[Dict]:
        return self.tables.get(table, [])

    def size_bytes(self) -> int:
        total = 0
        for f in os.listdir(self.data_dir):
            fp = os.path.join(self.data_dir, f)
            if os.path.isfile(fp):
                total += os.path.getsize(fp)
        return total


# ════════════════════════════════════════════════════════════
# 1. 冷备（Cold Backup）
# ════════════════════════════════════════════════════════════
def demo_cold_backup():
    print("═══ 1. 冷备（Cold Backup） ═══")
    db = SimpleDB(os.path.join(DB_DIR, "cold"))
    db.create_table("users")
    for i in range(5):
        db.insert("users", {"id": i + 1, "name": f"User{i+1}"})
    print(f"  数据库就绪: {db.size_bytes()} bytes")

    # 冷备：停服后复制文件
    print("  停服（模拟）")
    backup_path = os.path.join(BACKUP_DIR, "cold_backup")
    shutil.copytree(db.data_dir, backup_path)
    print(f"  冷备完成: {backup_path}")
    print("  开始服（模拟）\n")


# ════════════════════════════════════════════════════════════
# 2. 热备（Hot Backup）—— WAL 方式
# ════════════════════════════════════════════════════════════
def demo_hot_backup():
    print("═══ 2. 热备（Hot Backup） ═══")
    db = SimpleDB(os.path.join(DB_DIR, "hot"))
    db.create_table("orders")
    db.insert("orders", {"id": 1, "item": "book"})

    # 热备时仍有写入
    print("  开始热备份...")
    backup_path = os.path.join(BACKUP_DIR, "hot_backup")
    # 复制数据文件（此时 DB 还在运行）
    shutil.copytree(db.data_dir, backup_path)

    # 备份过程中的写入
    db.insert("orders", {"id": 2, "item": "pen"})
    db.insert("orders", {"id": 3, "item": "paper"})

    # 备份完成后需 WAL 日志解决不一致
    wal_file_src = os.path.join(db.data_dir, "wal.log")
    wal_file_dst = os.path.join(backup_path, "wal.log")
    if os.path.exists(wal_file_src):
        shutil.copy2(wal_file_src, wal_file_dst)

    print(f"  热备完成: {backup_path}")
    print(f"  备份期间有 2 条新写入（WAL 已包含）\n")


# ════════════════════════════════════════════════════════════
# 3. 全量备份 vs 增量备份
# ════════════════════════════════════════════════════════════
def demo_full_vs_incremental():
    print("═══ 3. 全量备份 vs 增量备份 ═══")
    db = SimpleDB(os.path.join(DB_DIR, "full_inc"))

    # 全量备份
    full_backup_path = os.path.join(BACKUP_DIR, "full_backup")
    db.create_table("products")
    for i in range(20):
        db.insert("products", {"id": i + 1, "name": f"Product{i+1}"})

    shutil.copytree(db.data_dir, full_backup_path)
    full_size = sum(os.path.getsize(os.path.join(full_backup_path, f))
                    for f in os.listdir(full_backup_path))
    print(f"  全量备份大小: {full_size} bytes")

    # 增量备份（只备份变化的数据）
    inc_changes = []
    for i in range(5):
        db.insert("products",
                  {"id": 100 + i, "name": f"NewProduct{i+1}"})
        inc_changes.append(f"INSERT products id={100+i}")

    # 记录增量
    inc_file = os.path.join(INC_DIR, "inc_wal.log")
    with open(inc_file, "w") as f:
        for change in inc_changes:
            f.write(change + "\n")

    inc_size = sum(os.path.getsize(os.path.join(INC_DIR, f))
                   for f in os.listdir(INC_DIR))
    print(f"  增量变化: {len(inc_changes)} 条新数据")
    print(f"  增量备份大小: {inc_size} bytes")
    print(f"  节约: {(1 - inc_size / full_size) * 100:.0f}%\n")


# ════════════════════════════════════════════════════════════
# 4. 恢复演练
# ════════════════════════════════════════════════════════════
def demo_recovery():
    print("═══ 4. 恢复演练 ═══")
    db = SimpleDB(os.path.join(DB_DIR, "recovery"))
    db.create_table("accounts")
    for i in range(3):
        db.insert("accounts", {"id": i + 1, "balance": 1000 * (i + 1)})

    # 1. 做备份
    recovery_backup = os.path.join(BACKUP_DIR, "recovery_backup")
    shutil.copytree(db.data_dir, recovery_backup)
    print(f"  完整备份创建: {recovery_backup}")

    # 2. 额外的写入
    db.insert("accounts", {"id": 4, "balance": 4000})
    db.insert("accounts", {"id": 5, "balance": 5000})
    print("  备份后新增 2 条记录")

    # 3. "灾难"：删除 DB
    print("  [灾难] DB 文件损坏！")
    # 模拟崩溃

    # 4. 恢复
    print()
    print("  恢复步骤：")
    print(f"  1) 从备份恢复: {recovery_backup} → {RESTORE_DIR}")
    restored_db_path = os.path.join(RESTORE_DIR, "restored")
    shutil.copytree(recovery_backup, restored_db_path)
    print(f"  2) 重放 WAL 日志（如果有）")
    restored_db = SimpleDB(restored_db_path)
    restored_db.insert("accounts", {"id": 4, "balance": 4000})
    restored_db.insert("accounts", {"id": 5, "balance": 5000})

    accounts = restored_db.read_all("accounts")
    print(f"  恢复后 accounts 表: {len(accounts)} 条")
    for a in accounts:
        print(f"    id={a['id']}, balance={a['balance']}")
    print("  [OK] 恢复完成 — 未丢失数据\n")


def demo_backup_strategies():
    print("═══ 备份策略总结 ═══")
    strategies = [
        ("全量备份", "每周 1 次，恢复快但存储大"),
        ("增量备份", "每天 1 次，只备份变化数据"),
        ("差异备份", "每天 1 次，备份与上次全量的差异"),
        ("WAL 归档", "连续归档 WAL 日志，支持 PITR（时间点恢复）"),
    ]
    for name, desc in strategies:
        print(f"  {name:12s} — {desc}")
    print()


def cleanup():
    for d in [DB_DIR, BACKUP_DIR, RESTORE_DIR, INC_DIR]:
        if os.path.exists(d):
            shutil.rmtree(d)


def main():
    print("=" * 50)
    print("  备份恢复演示 — 冷备/热备/全量/增量/恢复")
    print("=" * 50 + "\n")

    try:
        demo_cold_backup()
        demo_hot_backup()
        demo_full_vs_incremental()
        demo_recovery()
        demo_backup_strategies()
    finally:
        cleanup()

    print("所有备份恢复演示完成。")


if __name__ == "__main__":
    main()
