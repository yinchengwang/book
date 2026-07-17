# file_io 学习笔记

## 概念地图

Python 的文件 I/O 是数据持久化的基础：

- **文本 vs 二进制模式**：文本模式 `'t'` 自动编解码，二进制 `'b'` 保留原始字节
- **`with` 语句**：上下文管理器，自动关闭文件，防止资源泄漏
- **JSON**：轻量级数据交换格式，`ensure_ascii=False` 支持中文
- **CSV**：逗号分隔值，Excel 兼容，`DictReader` 便于按列名访问
- **pathlib**：Python 3.4+ 引入的面向对象路径库，取代 `os.path`

## 踩坑记录

1. **编码问题**：Windows 默认 GBK，跨平台代码用 `encoding='utf-8'`
2. **换行符**：Windows 写 CSV 用 `newline=''` 避免空行
3. **二进制文件**：`'rb'` 读图片/音频，不能用文本模式
4. **`json.load` vs `json.loads`**：前者读文件，后者读字符串
5. **文件指针**：`read()` 后指针在末尾，再读需 `seek(0)`

## 工程对照（≥100 字硬约束）

Python 文件 I/O 在 `learning/scripts/sync-pipeline.py` 和 `engineering/` 中有大量实践：

1. **配置文件读取**：`sync-pipeline.py` 用 `json.load()` 读取 `statuses.json` 和 `items-registry.js`
2. **JSON 序列化**：REST API 用 `json.dumps()` 序列化响应，`ensure_ascii=False` 支持中文
3. **CSV 日志导出**：数据分析用 `csv.DictWriter` 导出表格数据
4. **pathlib 路径拼接**：`Path(base_dir) / "subdir" / "file.txt"` 比 `os.path.join` 更直观
5. **临时文件**：`tempfile.mkdtemp()` 创建安全临时目录，避免文件名冲突
6. **配置文件热加载**：监控配置文件修改时间，`os.path.getmtime()` 检测变更

学完本卡能动手的事：实现一个配置文件管理器，支持 JSON/YAML 格式，自动热加载。
