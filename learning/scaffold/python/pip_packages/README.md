# pip_packages - Python pip 高级用法

## 概述

本卡演示 pip 包管理器的高级用法，包括依赖管理、版本锁定和缓存控制。

## 核心概念

| 概念 | 说明 |
|------|------|
| requirements.txt | 依赖清单文件，pip install -r 批量安装 |
| pip freeze | 导出当前环境所有包及精确版本 |
| pip check | 检查依赖冲突 |
| pip download | 仅下载不安装，用于离线部署 |
| pip hash | 生成包哈希值，用于安全校验 |

## 快速开始

```bash
# 运行演示
make run

# 或直接执行
bash main.sh
```

## 演示内容

1. **pip install 高级用法** - 版本约束、批量安装、pip show
2. **requirements.txt 管理** - pip freeze、依赖导出、分环境配置
3. **pip 配置与缓存** - 缓存目录、pip download
4. **pip 索引与搜索** - PyPI 镜像、pip hash

## 依赖

- Python 3.6+
- pip 21.0+
- Bash 4.0+

## 验证

```bash
# 检查环境
make test

# 查看帮助
make help
```

## 常见问题

**Q: pip install 报错 "externally-managed-environment"？**
A: Python 3.11+ 的 PEP 668，需要用虚拟环境或 `pip install --break-system-packages`。

**Q: 如何加速 pip 安装？**
A: 使用国内镜像：`pip install xxx -i https://pypi.tuna.tsinghua.edu.cn/simple`

**Q: pip freeze 和 requirements.txt 有什么区别？**
A: pip freeze 导出精确版本（==），requirements.txt 通常用兼容版本（>=）。
