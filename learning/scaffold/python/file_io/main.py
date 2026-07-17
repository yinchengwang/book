#!/usr/bin/env python3
# file_io scaffold — Python 文件 I/O
#
# 复现命令：
#   python3 main.py
#
# 演示 5 段：
#   [read_write]  — 读/写文件
#   [with]        — with 语句自动关闭
#   [json]        — JSON 序列化/反序列化
#   [csv]         — CSV 读写
#   [path]        — pathlib 路径操作

import os
import json
import csv
import tempfile
from pathlib import Path


def main():
    # 创建临时目录用于演示
    temp_dir = tempfile.mkdtemp()
    print(f"[setup] 临时目录: {temp_dir}")

    # === [read_write] 读/写文件 ===
    print("\n[read_write] === 读/写文件 ===")

    # 写入文件
    filepath = os.path.join(temp_dir, "test.txt")
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write("Hello, Python!\n")
        f.write("第二行内容\n")
        f.write("第三行内容\n")

    # 读取文件
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
        print(f"  read(): {content.strip().split(chr(10))[0]}")

    # 按行读取
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        print(f"  readlines(): {len(lines)} 行")

    # === [with] with 语句自动关闭 ===
    print("\n[with] === with 语句自动关闭 ===")

    filepath2 = os.path.join(temp_dir, "with_demo.txt")

    # with 语句自动调用 __enter__ 和 __exit__
    with open(filepath2, 'w') as f:
        f.write("with 语句自动关闭文件\n")
        print(f"  文件打开中... (退出 with 块时自动关闭)")

    # 检查文件是否已写入
    with open(filepath2, 'r') as f:
        print(f"  内容已写入: {f.read().strip()}")

    # === [json] JSON 序列化/反序列化 ===
    print("\n[json] === JSON 序列化/反序列化 ===")

    # 数据对象
    data = {
        "name": "Alice",
        "age": 30,
        "skills": ["Python", "C++", "SQL"],
        "active": True,
        "scores": {"math": 95, "english": 88}
    }

    # 序列化到文件
    json_path = os.path.join(temp_dir, "user.json")
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

    print(f"  写入 JSON: {json_path}")

    # 反序列化
    with open(json_path, 'r', encoding='utf-8') as f:
        loaded = json.load(f)

    print(f"  读取 JSON: name={loaded['name']}, age={loaded['age']}")
    print(f"  skills: {loaded['skills']}")

    # 字符串序列化
    json_str = json.dumps(data, ensure_ascii=False)
    print(f"  json.dumps(): {len(json_str)} 字符")

    # === [csv] CSV 读写 ===
    print("\n[csv] === CSV 读写 ===")

    csv_path = os.path.join(temp_dir, "users.csv")

    # 写入 CSV
    with open(csv_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(["name", "age", "city"])
        writer.writerow(["Alice", 30, "Beijing"])
        writer.writerow(["Bob", 25, "Shanghai"])
        writer.writerow(["Charlie", 35, "Guangzhou"])

    print(f"  写入 CSV: {csv_path}")

    # 读取 CSV
    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        users = list(reader)

    print(f"  读取 CSV: {len(users)} 行")
    for user in users[:2]:
        print(f"    {user['name']}, {user['age']}, {user['city']}")

    # === [path] pathlib 路径操作 ===
    print("\n[path] === pathlib 路径操作 ===")

    p = Path(csv_path)
    print(f"  Path('{p.name}')")
    print(f"    parent: {p.parent.name}")
    print(f"    stem: {p.stem}")
    print(f"    suffix: {p.suffix}")
    print(f"    exists(): {p.exists()}")

    # 列出目录下文件
    for f in Path(temp_dir).iterdir():
        print(f"    {f.name}")

    # 清理
    import shutil
    shutil.rmtree(temp_dir)
    print(f"\n[cleanup] 已删除临时目录")

    print("\n=== PASS ===")

if __name__ == "__main__":
    main()
