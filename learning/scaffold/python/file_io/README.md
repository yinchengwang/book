# file_io scaffold

Python 文件 I/O 演示——open/read/write + with 语句 + JSON/CSV + pathlib。

## 复现命令

```bash
cd learning/scaffold/python/file_io
python main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[read_write] === 读/写文件 ===
  read(): Hello, Python!

[json] === JSON 序列化/反序列化 ===
  读取 JSON: name=Alice, age=30
  skills: ['Python', 'C++', 'SQL']

[csv] === CSV 读写 ===
  读取 CSV: 3 行
    Alice, 30, Beijing

=== PASS ===
```

## 关键点

- **`open(file, mode)`**：mode 可选 r/w/a/r+/b/t
- **`with` 语句**：自动调用 `__exit__` 关闭文件
- **`json.dump/load`**：Python 对象与 JSON 字符串互转
- **`csv.reader/DictWriter`**：结构化表格数据处理
- **`pathlib.Path`**：面向对象的路径操作

详见 NOTES.md 工程对照段。
