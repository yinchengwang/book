# logging 学习笔记

## 概念地图

Python logging 是标准日志库，比 print 更适合生产环境：

- **Logger**：获取日志记录器 `logging.getLogger(name)`
- **Handler**：输出目标（StreamHandler、FileHandler、RotatingFileHandler）
- **Formatter**：输出格式 `%(asctime)s - %(name)s - %(levelname)s - %(message)s`
- **Level**：DEBUG/INFO/WARNING/ERROR/CRITICAL 五级

## 踩坑记录

1. **默认级别是 WARNING**：DEBUG/INFO 不输出，需设置级别
2. **多次调用 getLogger 同名**：返回同一实例，handler 可能累积
3. **FileHandler 编码问题**：中文环境指定 `encoding='utf-8'`
4. **多层 logger 的 handler 传播**：子 logger 默认继承父 logger 的 handler

## 工程对照（≥100 字硬约束）

logging 在本项目的工程实践中有多处应用：

### 1. sync-pipeline.py 的日志配置

`learning/scripts/sync-pipeline.py` 使用 logging 而非 print：

```python
import logging
import logging.config

# 配置日志格式和级别
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)
logger.info("开始同步...")
```

### 2. 工程中的结构化日志

工程代码中的日志配置模式：

```python
# engineering/src/ 中类似模式的 C 代码
# wal.c 中使用 fprintf(stderr, ...) 记录错误
# 对应 Python 工程应使用 logging.error()
```

### 3. 多环境日志配置

生产/开发环境不同的日志级别：

```python
# 开发环境：DEBUG 级别
# 生产环境：INFO 或 WARNING 级别
import os
level = logging.DEBUG if os.getenv('DEBUG') else logging.INFO
logging.basicConfig(level=level)
```

### 4. 日志文件轮转

生产环境使用 RotatingFileHandler 避免日志无限增长：

```python
from logging.handlers import RotatingFileHandler
handler = RotatingFileHandler(
    'app.log',
    maxBytes=10 * 1024 * 1024,  # 10MB
    backupCount=5
)
```

学完本卡能动手的事：在自己的 Python 项目中用 logging 替换 print，配置多 handler 和日志轮转。
