# Linux 下添加子模块命令

由于 Windows 路径长度限制和文件名限制，以下 3 个数据库无法在 Windows 上添加，需要在 Linux 环境下执行：

## 1. InfluxDB（时序数据库）

```bash
# Windows 无法添加原因：文件名包含冒号 ':'
# 错误示例：open(TSMA-rule-1:0-1): Permission denied
cd /path/to/book
git submodule add https://github.com/influxdata/influxdb.git reference/open-source/time_series/influxdb
git commit -m "chore: 添加 InfluxDB 子模块（时序数据库）"
git push origin main
```

## 2. Materialize（流数据库）

```bash
# Windows 无法添加原因：文件名 'nul.pt' 与 Windows 设备名冲突
cd /path/to/book
git submodule add https://github.com/MaterializeInc/materialize.git reference/open-source/streaming/materialize
git commit -m "chore: 添加 Materialize 子模块（流数据库）"
git push origin main
```

## 3. Apache Doris（分析数据库）

```bash
# Windows 无法添加原因：路径超过 260 字符
# 错误示例：docker/thirdparties/... 路径过长
cd /path/to/book
git submodule add https://github.com/apache/doris.git reference/open-source/analytical/doris
git commit -m "chore: 添加 Apache Doris 子模块（分析数据库）"
git push origin main
```

## 4. ClickHouse unshallow（可选）

ClickHouse 仓库太大（~10GB），在 Windows 上 unshallow 会超时。如需完整历史，可在 Linux 下执行：

```bash
cd /path/to/book/reference/open-source/analytical/clickhouse
git fetch --unshallow
cd /path/to/book
git add reference/open-source/analytical/clickhouse
git commit -m "chore: ClickHouse 子模块转为完整历史"
git push origin main
```

## 执行步骤

1. 在 Linux 环境下克隆 book 仓库
2. 执行上述命令添加子模块
3. 推送到 origin/main
4. Windows 环境下 `git pull --recurse-submodules` 同步

## 注意事项

- 所有命令在 `/path/to/book` 根目录执行
- 子模块路径遵循 `reference/open-source/<分类>/<名称>` 规范
- 分类命名：time_series, streaming, analytical
